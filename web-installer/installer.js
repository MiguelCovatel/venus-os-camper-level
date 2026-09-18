(function () {
  "use strict";

  const counterBase =
    "https://abacus.jasoncameron.dev";
  const counterPath =
    "miguelcovatel-venus-level/completed-installs";
  const counterElement = document.getElementById("install-count");
  const installButton = document.getElementById("start-install");
  const copyButton = document.getElementById("copy-command");
  const commandElement = document.getElementById("venus-command");
  let monitorTimer;
  let monitorDeadline = 0;
  let recordedThisFlash = false;

  async function counterRequest(action) {
    const controller = new AbortController();
    const timeout = window.setTimeout(() => controller.abort(), 5000);
    try {
      const response = await fetch(
        `${counterBase}/${action}/${counterPath}`,
        { cache: "no-store", signal: controller.signal },
      );
      if (!response.ok) {
        if (action === "get" && response.status === 404) return 0;
        throw new Error(`Counter returned ${response.status}`);
      }
      const payload = await response.json();
      if (!Number.isSafeInteger(payload.value) || payload.value < 0) {
        throw new Error("Counter returned an invalid value");
      }
      return payload.value;
    } finally {
      window.clearTimeout(timeout);
    }
  }

  function showCount(value) {
    if (counterElement) counterElement.textContent = value.toLocaleString("es-ES");
  }

  async function loadCount() {
    try {
      showCount(await counterRequest("get"));
    } catch (_error) {
      if (counterElement) counterElement.textContent = "No disponible";
    }
  }

  async function recordCompletedInstall() {
    if (recordedThisFlash) return;
    recordedThisFlash = true;
    try {
      showCount(await counterRequest("hit"));
    } catch (_error) {
      // The counter is optional. A network failure must never affect flashing.
    }
  }

  function deepText(root) {
    const roots = [root];
    let text = "";
    while (roots.length) {
      const current = roots.pop();
      text += ` ${current.textContent || ""}`;
      current.querySelectorAll?.("*").forEach((element) => {
        if (element.shadowRoot) roots.push(element.shadowRoot);
      });
    }
    return text;
  }

  function inspectInstaller() {
    if (deepText(document.body).includes("Installation complete!")) {
      window.clearInterval(monitorTimer);
      recordCompletedInstall();
    } else if (Date.now() > monitorDeadline) {
      window.clearInterval(monitorTimer);
    }
  }

  function startCompletionMonitor() {
    window.clearInterval(monitorTimer);
    recordedThisFlash = false;
    monitorDeadline = Date.now() + 15 * 60 * 1000;
    monitorTimer = window.setInterval(inspectInstaller, 400);
  }

  async function copyVenusCommand() {
    const command = commandElement?.textContent?.trim();
    if (!command || !copyButton) return;
    try {
      await navigator.clipboard.writeText(command);
    } catch (_error) {
      const area = document.createElement("textarea");
      area.value = command;
      area.style.position = "fixed";
      area.style.opacity = "0";
      document.body.appendChild(area);
      area.select();
      document.execCommand("copy");
      area.remove();
    }
    const previous = copyButton.textContent;
    copyButton.textContent = "Copiado ✓";
    window.setTimeout(() => {
      copyButton.textContent = previous;
    }, 1800);
  }

  installButton?.addEventListener("click", startCompletionMonitor);
  copyButton?.addEventListener("click", copyVenusCommand);
  loadCount();
})();
