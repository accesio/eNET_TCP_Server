(function () {
  "use strict";

  var writePageState = {
    writesEnabled: false,
    authEnabled: false,
    statusLoaded: false,
    busy: false,
    dio: null
  };

  function byId(id) { return document.getElementById(id); }

  function valueOrDash(value) {
    if (value === undefined || value === null || value === "") { return "--"; }
    return String(value);
  }

  function boolText(value) {
    if (value === true) { return "yes"; }
    if (value === false) { return "no"; }
    return valueOrDash(value);
  }

  function setText(id, value) {
    var element = byId(id);
    if (element) { element.textContent = valueOrDash(value); }
  }

  function setPill(id, text, className) {
    var element = byId(id);
    if (!element) { return; }
    element.className = "pill " + className;
    element.textContent = text;
  }

  function setWriteMessage(text, className) {
    var element = byId("dio-write-message");
    if (!element) { return; }
    element.className = "message " + className;
    element.textContent = text;
  }

  function updateWritePill() {
    if (!byId("dio-write-state")) { return; }
    if (writePageState.busy) {
      setPill("dio-write-state", "Applying", "pill-waiting");
    } else if (!writePageState.statusLoaded) {
      setPill("dio-write-state", "Loading", "pill-waiting");
    } else if (writePageState.writesEnabled) {
      setPill("dio-write-state", "Ready", "pill-ok");
    } else {
      setPill("dio-write-state", "Disabled", "pill-waiting");
    }
  }

  function formatUptime(ms) {
    if (typeof ms !== "number") { return "--"; }
    var seconds = Math.floor(ms / 1000);
    var days = Math.floor(seconds / 86400);
    seconds -= days * 86400;
    var hours = Math.floor(seconds / 3600);
    seconds -= hours * 3600;
    var minutes = Math.floor(seconds / 60);
    seconds -= minutes * 60;
    if (days > 0) { return days + "d " + hours + "h " + minutes + "m"; }
    if (hours > 0) { return hours + "h " + minutes + "m " + seconds + "s"; }
    return minutes + "m " + seconds + "s";
  }

  function fetchJson(path, options) {
    var requestOptions = options || {};
    requestOptions.cache = "no-store";
    requestOptions.credentials = "same-origin";

    return fetch(path, requestOptions).then(function (response) {
      return response.text().then(function (text) {
        var payload = {};
        if (text) {
          try {
            payload = JSON.parse(text);
          } catch (error) {
            throw new Error(path + " invalid JSON: " + error.message);
          }
        }

        if (!response.ok) {
          var detail = payload.detail || payload.error || text || ("HTTP " + response.status);
          throw new Error(path + " " + detail);
        }

        return payload;
      });
    });
  }

  function postJson(path, payload) {
    return fetchJson(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload || {})
    });
  }

  function parseHexNumber(value) {
    if (typeof value === "number") { return value; }
    var text = String(valueOrDash(value)).replace(/^0x/i, "");
    var parsed = parseInt(text, 16);
    if (isNaN(parsed)) { return 0; }
    return parsed;
  }

  function bitIsSet(value, bit) {
    return ((value & Math.pow(2, bit)) !== 0);
  }

  function byteToHex(value) {
    var text = (value & 0xFF).toString(16).toUpperCase();
    return text.length < 2 ? "0" + text : text;
  }

  function bytesToHex(bytes) {
    var out = "0x";
    for (var i = bytes.length - 1; i >= 0; --i) {
      out += byteToHex(bytes[i] || 0);
    }
    return out;
  }

  function groupBitCount(group) {
    var count = Number(group && group.bit_count);
    if (!isFinite(count) || count <= 0) { return 8; }
    return Math.min(count, 16);
  }

  function groupFirstBit(group) {
    var first = Number(group && group.first_bit);
    if (isFinite(first) && first >= 0) { return first; }
    return Number(group.group || 0) * 8;
  }

  function singleBitMaskHex(group, bit, dio) {
    var totalBits = Number(dio && dio.bits);
    if (!isFinite(totalBits) || totalBits <= 0) { totalBits = 48; }
    var byteCount = Math.max(1, Math.ceil(totalBits / 8));
    var absoluteBit = groupFirstBit(group) + bit;
    var bytes = [];
    for (var i = 0; i < byteCount; ++i) { bytes[i] = 0; }
    bytes[Math.floor(absoluteBit / 8)] = (1 << (absoluteBit % 8)) & 0xFF;
    return bytesToHex(bytes);
  }

  function makeButton(text, className, disabled, handler) {
    var button = document.createElement("button");
    button.type = "button";
    button.className = className;
    button.textContent = text;
    button.disabled = !!disabled;
    if (handler) { button.addEventListener("click", handler); }
    return button;
  }

  function renderStatus(payload) {
    var unit = payload && payload.unit ? payload.unit : {};
    var jumpers = payload && payload.jumpers ? payload.jumpers : {};
    var api = payload && payload.api ? payload.api : {};
    var capabilities = payload && payload.capabilities ? payload.capabilities : {};
    var dioCaps = capabilities.dio || {};

    setText("unit-uid", unit.uid);
    setText("unit-model", unit.model);
    setText("unit-model-code", unit.model_code);
    setText("unit-revision", unit.revision);
    setText("unit-firmware", unit.firmware);
    setText("unit-comm-mode", unit.comm_mode);
    setText("unit-uptime", formatUptime(unit.uptime_ms));

    setText("jumpers-mask", jumpers.active_mask);
    setText("jumpers-opt0", boolText(jumpers.opt0));
    setText("jumpers-opt1", boolText(jumpers.opt1));
    setText("jumpers-opt2", boolText(jumpers.opt2));
    setText("jumpers-opt3", boolText(jumpers.opt3));
    setText("jumpers-network-mode", jumpers.network_mode);
    setText("jumpers-update-enabled", boolText(jumpers.update_enabled));

    setText("api-version", api.version);
    setText("api-phase", api.phase);
    setText("api-writes-enabled", boolText(api.writes_enabled));
    setText("api-auth-enabled", boolText(api.auth_enabled));
    setText("cap-dio-bits", dioCaps.bits);
    setText("cap-dio-groups", dioCaps.group_count);
    setText("cap-dio-group-bits", dioCaps.max_bits_per_group);
    setText("last-update", new Date().toLocaleTimeString());
    setPill("connection-state", "Connected", "pill-ok");

    writePageState.statusLoaded = true;
    writePageState.writesEnabled = (api.writes_enabled === true);
    writePageState.authEnabled = (api.auth_enabled === true);
    updateWritePill();

    if (byId("dio-write-message") && !writePageState.busy) {
      if (writePageState.writesEnabled) {
        setWriteMessage("Writes are enabled. Direction and output-bit controls send immediately.", "message-ok");
      } else {
        setWriteMessage("Write endpoints are compiled in but disabled. Set ETHDIO_WEB_CONTROL_WRITES_ENABLE to 1 to enable these controls.", "message-muted");
      }
    }

    if (writePageState.dio) { renderWriteControls(writePageState.dio); }
  }

  function renderIo(payload) {
    var dio = payload && payload.dio ? payload.dio : {};
    var tbody = byId("dio-groups");

    writePageState.dio = dio;

    setText("dio-input-mask", dio.input_mask);
    setText("dio-output-mask", dio.output_mask);
    setText("dio-physical-state", dio.physical_state);
    setText("dio-output-latch", dio.output_latch);

    if (tbody) {
      tbody.innerHTML = "";
      (dio.groups || []).forEach(function (group) {
        var tr = document.createElement("tr");
        [group.group, group.direction, group.bit_count, group.pins, group.output_latch].forEach(function (value) {
          var td = document.createElement("td");
          td.textContent = valueOrDash(value);
          tr.appendChild(td);
        });
        tbody.appendChild(tr);
      });
      if (!tbody.children.length) {
        var empty = document.createElement("tr");
        var td = document.createElement("td");
        td.colSpan = 5;
        td.textContent = "No DIO data";
        empty.appendChild(td);
        tbody.appendChild(empty);
      }
    }

    renderWriteControls(dio);
    setPill("dio-state", "Live", "pill-ok");
  }

  function renderWriteControls(dio) {
    var container = byId("dio-write-controls");
    if (!container) { return; }

    var groups = dio && dio.groups ? dio.groups : [];
    var canWrite = writePageState.writesEnabled && !writePageState.busy;
    container.innerHTML = "";

    if (!groups.length) {
      container.textContent = "No DIO data";
      updateWritePill();
      return;
    }

    groups.forEach(function (group) {
      var groupNumber = Number(group.group);
      var direction = String(group.direction || "input").toLowerCase();
      var isOutput = (direction === "output");
      var latch = parseHexNumber(group.output_latch);
      var pins = parseHexNumber(group.pins);
      var bitsInGroup = groupBitCount(group);

      var card = document.createElement("section");
      card.className = "write-group-card";

      var header = document.createElement("div");
      header.className = "write-group-header";

      var titleWrap = document.createElement("div");
      var title = document.createElement("h3");
      title.className = "write-group-title";
      title.textContent = "Group " + groupNumber;
      var meta = document.createElement("p");
      meta.className = "write-group-meta";
      meta.textContent = "direction=" + direction + "  bits=" + bitsInGroup + "  pins=" + valueOrDash(group.pins) + "  latch=" + valueOrDash(group.output_latch);
      titleWrap.appendChild(title);
      titleWrap.appendChild(meta);

      var directionButtons = document.createElement("div");
      directionButtons.className = "direction-buttons";

      var inputClass = "control-button" + (!isOutput ? " active" : "");
      var outputClass = "control-button" + (isOutput ? " active" : "");
      directionButtons.appendChild(makeButton("Input", inputClass, !canWrite || !isOutput, function () {
        setGroupDirection(groupNumber, "input");
      }));
      directionButtons.appendChild(makeButton("Output", outputClass, !canWrite || isOutput, function () {
        setGroupDirection(groupNumber, "output");
      }));

      header.appendChild(titleWrap);
      header.appendChild(directionButtons);
      card.appendChild(header);

      var bitGrid = document.createElement("div");
      bitGrid.className = "bit-grid";

      for (var bit = 0; bit < bitsInGroup; ++bit) {
        (function (bitIndex) {
          var latchHigh = bitIsSet(latch, bitIndex);
          var pinHigh = bitIsSet(pins, bitIndex);
          var bitControl = document.createElement("div");
          bitControl.className = "bit-control" + (isOutput ? "" : " read-only");

          var bitTitle = document.createElement("div");
          bitTitle.className = "bit-title";
          var bitLabel = document.createElement("span");
          bitLabel.textContent = "Bit " + (groupFirstBit(group) + bitIndex);
          var state = document.createElement("span");
          state.className = "bit-state";
          state.textContent = isOutput ? (latchHigh ? "latch high" : "latch low") : (pinHigh ? "pin high" : "pin low");
          bitTitle.appendChild(bitLabel);
          bitTitle.appendChild(state);

          var actions = document.createElement("div");
          actions.className = "bit-actions";
          actions.appendChild(makeButton("Low", "control-button low" + (!latchHigh ? " active" : ""), !canWrite || !isOutput || !latchHigh, function () {
            setOutputBit(group, bitIndex, false);
          }));
          actions.appendChild(makeButton("High", "control-button high" + (latchHigh ? " active" : ""), !canWrite || !isOutput || latchHigh, function () {
            setOutputBit(group, bitIndex, true);
          }));

          bitControl.appendChild(bitTitle);
          bitControl.appendChild(actions);
          bitGrid.appendChild(bitControl);
        }(bit));
      }

      card.appendChild(bitGrid);
      container.appendChild(card);
    });

    updateWritePill();
  }

  function loadDioOnce() {
    return fetchJson("/api/v1/io").then(function (payload) {
      renderIo(payload);
      return payload;
    });
  }

  function finishWriteSuccess(message) {
    writePageState.busy = false;
    updateWritePill();
    renderWriteControls(writePageState.dio || {});
    setWriteMessage(message, "message-ok");
  }

  function finishWriteError(error) {
    writePageState.busy = false;
    updateWritePill();
    renderWriteControls(writePageState.dio || {});
    setWriteMessage(error && error.message ? error.message : "write failed", "message-error");
  }

  function beginWrite(message) {
    writePageState.busy = true;
    updateWritePill();
    setWriteMessage(message, "message-muted");
    renderWriteControls(writePageState.dio || {});
  }

  function setGroupDirection(group, direction) {
    if (!writePageState.writesEnabled || writePageState.busy) { return; }

    beginWrite("Setting group " + group + " to " + direction + "...");
    postJson("/api/v1/io/direction", { group: group, direction: direction }).then(function (payload) {
      if (payload && payload.dio) {
        renderIo(payload);
        return payload;
      }
      return loadDioOnce();
    }).then(function () {
      finishWriteSuccess("Group " + group + " is now " + direction + ".");
    }, finishWriteError);
  }

  function setOutputBit(group, bit, high) {
    if (!writePageState.writesEnabled || writePageState.busy) { return; }

    var groupNumber = Number(group.group);
    var absoluteBit = groupFirstBit(group) + bit;
    var mask = singleBitMaskHex(group, bit, writePageState.dio);
    var value = high ? mask : "0x0";
    beginWrite("Setting DIO bit " + absoluteBit + " " + (high ? "high" : "low") + "...");

    postJson("/api/v1/io/outputs", { mask: mask, value: value }).then(function (payload) {
      if (payload && payload.notes && payload.notes.length) {
        setWriteMessage(payload.notes.map(function (note) { return note.detail || note.code; }).join("; "), "message-muted");
      }
      return loadDioOnce();
    }).then(function () {
      finishWriteSuccess("Group " + groupNumber + " / DIO bit " + absoluteBit + " set " + (high ? "high" : "low") + ".");
    }, finishWriteError);
  }

  function renderNetwork(payload) {
    var network = payload && payload.network ? payload.network : {};
    var ethernet = network.ethernet || {};
    var usb = network.usb || {};
    var pending = network.pending || {};

    setText("network-active-interface", network.active_interface);
    setText("network-policy-mode", network.policy_mode);
    setText("network-eth-link", boolText(ethernet.link));
    setText("network-dhcp", boolText(ethernet.dhcp));
    setText("network-ipv4", ethernet.ipv4);
    setText("network-netmask", ethernet.netmask);
    setText("network-gateway", ethernet.gateway);
    setText("network-mac", ethernet.mac);
    setText("usb-product", usb.product || (usb.present ? "present" : "--"));
    setText("network-pending", pending.valid ? "yes" : "no");
    setPill("network-state", ethernet.link ? "Link up" : "No link", ethernet.link ? "pill-ok" : "pill-waiting");
  }


  function renderCapabilities(payload) {
    var caps = payload && payload.capabilities ? payload.capabilities : {};
    var dio = caps.dio || {};
    setText("cap-dio-bits", dio.bits);
    setText("cap-dio-groups", dio.group_count);
    setText("cap-dio-group-bits", dio.max_bits_per_group);
  }

  function renderSystem(payload) {
    var system = payload && payload.system ? payload.system : {};
    setText("system-architecture", system.architecture);
    setText("system-rtos", system.rtos);
    setText("system-tcpip", system.tcpip_stack);
    setText("system-webcontrol", system.web_control);
  }

  function renderMainError(error) {
    setPill("connection-state", "Error", "pill-error");
    setText("api-phase", error && error.message ? error.message : "request failed");
    setText("last-update", new Date().toLocaleTimeString());
  }

  function loadMainPage() {
    if (!byId("unit-uid")) { return; }

    fetchJson("/api/v1/status").then(renderStatus).catch(renderMainError);
    fetchJson("/api/v1/io").then(renderIo).catch(function (error) {
      setPill("dio-state", "Error", "pill-error");
      setText("dio-physical-state", error.message);
    });
    fetchJson("/api/v1/network").then(renderNetwork).catch(function (error) {
      setPill("network-state", "Error", "pill-error");
      setText("network-ipv4", error.message);
    });
    fetchJson("/api/v1/capabilities").then(renderCapabilities).catch(function () {});
    fetchJson("/api/v1/system").then(renderSystem).catch(function () {});
  }

  function loadWritePage() {
    if (!byId("dio-write-controls")) { return; }

    fetchJson("/api/v1/status").then(renderStatus).catch(function (error) {
      writePageState.statusLoaded = false;
      writePageState.writesEnabled = false;
      updateWritePill();
      setWriteMessage(error && error.message ? error.message : "status request failed", "message-error");
    });

    fetchJson("/api/v1/io").then(renderIo).catch(function (error) {
      setPill("dio-state", "Error", "pill-error");
      setPill("dio-write-state", "Error", "pill-error");
      setText("dio-physical-state", error.message);
      setWriteMessage(error && error.message ? error.message : "DIO request failed", "message-error");
    });
  }

  function renderHttps(payload) {
    var https = payload && payload.https ? payload.https : {};
    setText("https-compiled", boolText(https.netx_https_compiled));
    setText("https-config-enabled", boolText(https.configuration_enabled));
    setText("https-http-port", https.http_port);
    setText("https-port", https.https_port);
    setText("https-running", boolText(https.server_running));
    setText("https-status", https.status);
  }

  function renderCertificates(payload) {
    var certs = payload && payload.certificates ? payload.certificates : {};
    setText("cert-installed", boolText(certs.server_certificate_installed));
    setText("key-installed", boolText(certs.server_private_key_installed));
    setText("ca-count", certs.trusted_ca_count);
    setText("upload-enabled", boolText(certs.upload_enabled));
    setText("cert-storage", certs.storage);
    setText("cert-last-update", "Last update: " + new Date().toLocaleTimeString());
    setPill("cert-state", "Loaded", "pill-ok");
  }

  function loadCertificatePage() {
    if (!byId("https-compiled")) { return; }

    fetchJson("/api/v1/https").then(renderHttps).catch(function (error) {
      setPill("cert-state", "Error", "pill-error");
      setText("https-status", error.message);
    });
    fetchJson("/api/v1/certificates").then(renderCertificates).catch(function (error) {
      setPill("cert-state", "Error", "pill-error");
      setText("cert-storage", error.message);
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    loadMainPage();
    loadWritePage();
    loadCertificatePage();
    window.setInterval(loadMainPage, 3000);
    window.setInterval(loadWritePage, 2000);
    window.setInterval(loadCertificatePage, 5000);
  });
}());
