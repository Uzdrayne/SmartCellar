(function (global) {
  const stateListeners = new Set();
  const connectionListeners = new Set();
  const eventListeners = new Map();
  const pendingRequests = new Map();
  let socket = null;
  let lastState = null;
  let rawState = null;
  // Les limites sont constantes : demandées une fois puis fusionnées dans
  // chaque état pour que les pages continuent de lire state.Limits.
  let limits = null;
  let limitsSettled = false;
  let limitsRequestInProgress = false;
  let reconnectTimer = null;
  let reconnectDelay = 1000;
  let isConnected = false;

  function updateConnectionBanner(connected) {
    if (!global.document || !global.document.body) return;

    let banner = global.document.getElementById("websocket-connection-banner");
    if (!banner) {
      banner = global.document.createElement("div");
      banner.id = "websocket-connection-banner";
      banner.className = "websocket-connection-banner";
      banner.setAttribute("role", "status");
      banner.setAttribute("aria-live", "polite");
      global.document.body.prepend(banner);
    }

    banner.textContent = connected
      ? ""
      : "Connexion WebSocket interrompue. Les données peuvent ne pas être à jour; reconnexion automatique en cours.";
    banner.hidden = connected;
  }

  function notifyConnection(connected) {
    updateConnectionBanner(connected);
    if (isConnected === connected) return;
    isConnected = connected;
    connectionListeners.forEach(listener => listener(connected));
  }

  function requestId() {
    if (global.crypto && typeof global.crypto.randomUUID === "function") {
      return global.crypto.randomUUID();
    }
    return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`;
  }

  function failPendingRequests(message) {
    pendingRequests.forEach((request, id) => {
      clearTimeout(request.timer);
      const error = new Error(message);
      error.code = "connection_lost";
      request.reject(error);
      pendingRequests.delete(id);
    });
  }

  // Une erreur dans un écouteur ne doit pas empêcher les autres d'être appelés.
  function callListener(listener, value) {
    try {
      listener(value);
    } catch (error) {
      console.error("Erreur dans un écouteur WebSocket :", error);
    }
  }

  function dispatchState() {
    if (!rawState || !limitsSettled) return;
    lastState = limits ? Object.assign({}, rawState, { Limits: limits }) : rawState;
    stateListeners.forEach(listener => callListener(listener, lastState));
  }

  function loadLimits() {
    if (limits || limitsRequestInProgress) return;
    limitsRequestInProgress = true;
    request("getLimits", {})
      .then(result => { limits = result; })
      .catch(error => console.error("Limites de configuration indisponibles :", error))
      .finally(() => {
        limitsRequestInProgress = false;
        limitsSettled = true;
        dispatchState();
      });
  }

  function connect() {
    if (socket && (socket.readyState === WebSocket.CONNECTING ||
                   socket.readyState === WebSocket.OPEN)) {
      return;
    }

    const scheme = global.location.protocol === "https:" ? "wss:" : "ws:";
    const currentSocket = new WebSocket(
      `${scheme}//${global.location.host}/ws`
    );
    socket = currentSocket;

    currentSocket.addEventListener("open", () => {
      if (socket !== currentSocket) return;
      reconnectDelay = 1000;
      notifyConnection(true);
      loadLimits();
    });

    currentSocket.addEventListener("message", event => {
      if (socket !== currentSocket) return;
      let message;
      try {
        message = JSON.parse(event.data);
      } catch (error) {
        console.error("Message WebSocket JSON invalide :", error);
        return;
      }

      if (message.version !== 1) {
        console.error("Version de protocole WebSocket non prise en charge.");
        return;
      }
      if (message.type === "state" && message.payload) {
        rawState = message.payload;
        dispatchState();
        return;
      }
      if (message.type === "event" && typeof message.event === "string") {
        const listeners = eventListeners.get(message.event);
        if (listeners) listeners.forEach(listener => listener(message));
        return;
      }
      if (message.type !== "response" || typeof message.id !== "string") {
        console.error("Enveloppe WebSocket inconnue.");
        return;
      }

      const request = pendingRequests.get(message.id);
      if (!request) return;
      clearTimeout(request.timer);
      pendingRequests.delete(message.id);
      if (!message.ok) {
        const error = new Error(
          message.error && message.error.message
            ? message.error.message
            : "La commande a échoué."
        );
        error.code = message.error && message.error.code;
        error.result = message.result || null;
        request.reject(error);
        return;
      }
      request.resolve(message.result || {});
    });

    currentSocket.addEventListener("close", () => {
      if (socket !== currentSocket) return;
      socket = null;
      notifyConnection(false);
      failPendingRequests(
        "Connexion interrompue; le résultat de la commande est inconnu."
      );
      if (reconnectTimer === null) {
        const delay = reconnectDelay;
        reconnectDelay = Math.min(reconnectDelay * 2, 30000);
        reconnectTimer = global.setTimeout(() => {
          reconnectTimer = null;
          connect();
        }, delay);
      }
    });

    currentSocket.addEventListener("error", () => {
      if (socket !== currentSocket) return;
      console.error(
        "Échec de connexion WebSocket; vérifiez l'authentification HTTP et la disponibilité du contrôleur."
      );
      currentSocket.close();
    });
  }

  function request(action, payload) {
    return new Promise((resolve, reject) => {
      if (!socket || socket.readyState !== WebSocket.OPEN) {
        reject(new Error("Le WebSocket n'est pas connecté."));
        return;
      }

      const id = requestId();
      const message = JSON.stringify({
        version: 1,
        id,
        action,
        payload: payload || {}
      });
      const timer = global.setTimeout(() => {
        pendingRequests.delete(id);
        const error = new Error(
          "Délai dépassé; le résultat de la commande est inconnu."
        );
        error.code = "timeout";
        reject(error);
      }, 10000);
      pendingRequests.set(id, { resolve, reject, timer });
      try {
        socket.send(message);
      } catch (error) {
        clearTimeout(timer);
        pendingRequests.delete(id);
        reject(error);
      }
    });
  }

  function onState(listener) {
    stateListeners.add(listener);
    // Appel différé : la page qui s'abonne n'a peut-être pas fini d'exécuter
    // son script (constantes déclarées plus bas).
    if (lastState) {
      global.setTimeout(() => {
        if (stateListeners.has(listener) && lastState) callListener(listener, lastState);
      }, 0);
    }
    return () => stateListeners.delete(listener);
  }

  function onConnection(listener) {
    connectionListeners.add(listener);
    listener(isConnected);
    return () => connectionListeners.delete(listener);
  }

  function onEvent(name, listener) {
    if (!eventListeners.has(name)) eventListeners.set(name, new Set());
    const listeners = eventListeners.get(name);
    listeners.add(listener);
    return () => listeners.delete(listener);
  }

  global.SmartCellarWebSocket = {
    request,
    onState,
    onConnection,
    onEvent
  };

  connect();
})(window);
