const reportedLimitErrors = new WeakSet();

function applyNumericLimits(input, limits) {
  const valid = input &&
    limits &&
    typeof limits.Min === "number" &&
    Number.isFinite(limits.Min) &&
    typeof limits.Max === "number" &&
    Number.isFinite(limits.Max) &&
    limits.Min <= limits.Max;

  if (!valid) {
    if (input) {
      input.disabled = true;
      if (!reportedLimitErrors.has(input)) {
        console.error("Limites numériques absentes ou invalides.");
        reportedLimitErrors.add(input);
      }
    }
    return false;
  }

  input.min = String(limits.Min);
  input.max = String(limits.Max);
  input.disabled = false;
  reportedLimitErrors.delete(input);
  return true;
}

function isWithinNumericLimits(value, limits) {
  return Number.isFinite(value) &&
    limits &&
    Number.isFinite(limits.Min) &&
    Number.isFinite(limits.Max) &&
    limits.Min <= limits.Max &&
    value >= limits.Min &&
    value <= limits.Max;
}

function formatNumericLimits(limits) {
  if (!limits ||
      !Number.isFinite(limits.Min) ||
      !Number.isFinite(limits.Max) ||
      limits.Min > limits.Max) {
    return "indisponibles";
  }
  return `${limits.Min} à ${limits.Max}`;
}
