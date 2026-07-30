var MQTT_WS_URL = "ws://192.168.1.200:8083/mqtt";
var TOPIC_TEMP = "smart-home/dht11/temperature";
var TOPIC_HUMI = "smart-home/dht11/humidity";
var TOPIC_LED = "smart-home/led";

var task = null;
var pingTimer = null;
var reconnectTimer = null;

Page({
  data: {
    temperature: "--",
    humidity: "--",
    ledOn: false,
    connected: false
  },

  onLoad: function() { this.connectMQTT(); },
  onUnload: function() { this.disconnectMQTT(); },

  connectMQTT: function() {
    var that = this;
    if (task) { task.close({ code: 1000 }); task = null; }

    task = wx.connectSocket({
      url: MQTT_WS_URL,
      success: function() { console.log("WS connecting..."); },
      fail: function() { that.scheduleReconnect(); }
    });

    task.onOpen(function() {
      console.log("WS open, sending MQTT CONNECT");
      task.send({ data: buildConnectPacket() });
    });

    task.onMessage(function(res) {
      var buf = res.data;
      if (typeof buf === "string") {
        buf = new Uint8Array(buf.split("").map(function(c) { return c.charCodeAt(0); })).buffer;
      }
      that.handleMQTT(buf);
    });

    task.onClose(function() {
      that.setData({ connected: false });
      clearInterval(pingTimer);
      that.scheduleReconnect();
    });

    task.onError(function() { that.scheduleReconnect(); });
  },

  handleMQTT: function(data) {
    var dv = new DataView(data);
    var ptype = (dv.getUint8(0) >> 4) & 0x0F;
    if (ptype === 2) {
      if (dv.getUint8(2) === 0) {
        console.log("MQTT connected");
        this.setData({ connected: true });
        this.subscribeTopics();
        var that = this;
        pingTimer = setInterval(function() {
          if (task) task.send({ data: new Uint8Array([0xC0, 0x00]).buffer });
        }, 30000);
      } else {
        this.scheduleReconnect();
      }
    } else if (ptype === 3) {
      this.handlePublish(data);
    } else if (ptype === 13) {
      console.log("PINGRESP");
    }
  },

  handlePublish: function(data) {
    var dv = new DataView(data);
    var pos = 1, multiplier = 1, remLen = 0;
    while (true) {
      var b = dv.getUint8(pos);
      remLen += (b & 0x7F) * multiplier;
      multiplier *= 128; pos++;
      if ((b & 0x80) === 0) break;
    }
    var topicLen = dv.getUint16(pos); pos += 2;
    var topic = "";
    for (var i = 0; i < topicLen; i++) {
      topic += String.fromCharCode(dv.getUint8(pos + i));
    }
    pos += topicLen;
    var payload = "";
    for (var j = pos; j < data.byteLength; j++) {
      payload += String.fromCharCode(dv.getUint8(j));
    }
    console.log("MQTT recv:", topic, payload);
    if (topic === TOPIC_TEMP) this.setData({ temperature: payload });
    else if (topic === TOPIC_HUMI) this.setData({ humidity: payload });
  },

  subscribeTopics: function() {
    var pktId = Math.floor(Math.random() * 65535);
    var topics = [TOPIC_TEMP, TOPIC_HUMI];
    var payload = [];
    for (var i = 0; i < topics.length; i++) {
      var t = topics[i];
      payload.push((t.length >> 8) & 0xFF, t.length & 0xFF);
      for (var j = 0; j < t.length; j++) payload.push(t.charCodeAt(j));
      payload.push(0);
    }
    var varHeader = [(pktId >> 8) & 0xFF, pktId & 0xFF];
    var total = varHeader.length + payload.length;
    var remBytes = encodeRemLen(total);
    var packet = [0x82].concat(remBytes).concat(varHeader).concat(payload);
    if (task) task.send({ data: new Uint8Array(packet).buffer });
  },

  onLedToggle: function(e) {
    var on = e.detail.value;
    this.setData({ ledOn: on });
    var cmd = on ? "ON" : "OFF";
    var pkt = buildPublishBytes(TOPIC_LED, cmd);
    if (task) task.send({ data: new Uint8Array(pkt).buffer });
  },

  scheduleReconnect: function() {
    var that = this;
    clearTimeout(reconnectTimer);
    clearInterval(pingTimer);
    reconnectTimer = setTimeout(function() { that.connectMQTT(); }, 5000);
  },

  disconnectMQTT: function() {
    clearTimeout(reconnectTimer);
    clearInterval(pingTimer);
    if (task) { task.close({ code: 1000 }); task = null; }
  }
});

function encodeRemLen(length) {
  var bytes = [];
  do {
    var b = length % 128;
    length = Math.floor(length / 128);
    if (length > 0) b |= 0x80;
    bytes.push(b);
  } while (length > 0);
  return bytes;
}

function buildConnectPacket() {
  var cid = "wx_" + Date.now();
  var payload = [];
  payload.push((cid.length >> 8) & 0xFF, cid.length & 0xFF);
  for (var i = 0; i < cid.length; i++) payload.push(cid.charCodeAt(i));
  var varHeader = [0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04, 0x02, 0x00, 0x3C];
  var total = varHeader.length + payload.length;
  var remBytes = encodeRemLen(total);
  return new Uint8Array([0x10].concat(remBytes).concat(varHeader).concat(payload)).buffer;
}

function buildPublishBytes(topic, payload) {
  var body = [];
  body.push((topic.length >> 8) & 0xFF, topic.length & 0xFF);
  for (var i = 0; i < topic.length; i++) body.push(topic.charCodeAt(i));
  for (var j = 0; j < payload.length; j++) body.push(payload.charCodeAt(j));
  var remBytes = encodeRemLen(body.length);
  return [0x30].concat(remBytes).concat(body);
}