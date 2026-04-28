/**
 * main.js — PlagiaCheck Frontend Logic
 * ──────────────────────────────────────────────────────────────────────────
 * Handles:
 *   1. File-count stepper → renders N upload slots dynamically
 *   2. Sends FormData to POST /analyze
 *   3. Renders Chart.js bar graph from the response
 *   4. Highlights matched text inside each result card
 *   5. Expandable per-file cards (click to open/close)
 */

// ── State ─────────────────────────────────────────────────────────────────

let chartInstance = null;   // keeps reference so we can destroy & redraw

// ── Element References ────────────────────────────────────────────────────

const sourceFileInput  = document.getElementById("source-file");
const sourceChosen     = document.getElementById("source-chosen");
const fileCountInput   = document.getElementById("file-count-input");
const targetSlotsWrap  = document.getElementById("target-slots");
const analyzeBtn       = document.getElementById("analyze-btn");
const errorBanner      = document.getElementById("error-banner");
const loader           = document.getElementById("loader");
const resultsSection   = document.getElementById("results-section");
const resultsList      = document.getElementById("results-list");

// ── Source file label ─────────────────────────────────────────────────────

sourceFileInput.addEventListener("change", () => {
  const file = sourceFileInput.files[0];
  sourceChosen.textContent = file ? `✓ ${file.name}` : "";

  // Also update the drop-zone visuals so it's obvious a file was picked
  const icon  = document.getElementById("source-dz-icon");
  const title = document.getElementById("source-dz-title");
  if (icon && title) {
    if (file) {
      icon.textContent  = "✅";
      title.textContent = file.name;
      title.style.color = "var(--green)";
    } else {
      icon.textContent  = "📄";
      title.textContent = "Click or drag to upload";
      title.style.color = "";
    }
  }
});

// Drag-over visual feedback
const sourceDrop = document.getElementById("source-drop");
sourceDrop.addEventListener("dragover", e => { e.preventDefault(); sourceDrop.classList.add("drag-over"); });
sourceDrop.addEventListener("dragleave", ()  => sourceDrop.classList.remove("drag-over"));
sourceDrop.addEventListener("drop",     ()  => sourceDrop.classList.remove("drag-over"));

// ── File-count stepper ────────────────────────────────────────────────────

document.getElementById("count-down").addEventListener("click", () => {
  const current = parseInt(fileCountInput.value, 10) || 1;
  if (current > 1) fileCountInput.value = current - 1;
  renderSlots();
});

document.getElementById("count-up").addEventListener("click", () => {
  const current = parseInt(fileCountInput.value, 10) || 1;
  if (current < 20) fileCountInput.value = current + 1;
  renderSlots();
});

fileCountInput.addEventListener("change", renderSlots);

/** Rebuild target upload slots whenever the count changes. */
function renderSlots() {
  const count = Math.max(1, Math.min(20, parseInt(fileCountInput.value, 10) || 1));
  fileCountInput.value = count;   // clamp
  targetSlotsWrap.innerHTML = "";

  for (let i = 1; i <= count; i++) {
    const slot     = document.createElement("div");
    slot.className = "target-slot";
    slot.innerHTML = `
      <span class="slot-num">#${i}</span>
      <input type="file" accept=".txt" id="target-slot-${i}" aria-label="Comparison file ${i}" />
      <span class="slot-status" id="slot-status-${i}"></span>
    `;
    targetSlotsWrap.appendChild(slot);

    // Show filename when user picks a file
    slot.querySelector(`#target-slot-${i}`).addEventListener("change", function () {
      const statusEl = document.getElementById(`slot-status-${i}`);
      statusEl.textContent = this.files[0] ? `✓ ${this.files[0].name}` : "";
    });
  }
}

// Render default 3 slots on page load
renderSlots();

// ── Analyze ───────────────────────────────────────────────────────────────

analyzeBtn.addEventListener("click", startAnalysis);

async function startAnalysis() {
  hideError();
  hideResults();

  // ── Validate inputs ──────────────────────────────────────────────────────

  if (!sourceFileInput.files[0]) {
    showError("Please upload a source file first.");
    return;
  }

  // Collect all chosen target files (skip empty slots)
  const targetFiles = [];
  const count = parseInt(fileCountInput.value, 10) || 1;
  for (let i = 1; i <= count; i++) {
    const input = document.getElementById(`target-slot-${i}`);
    if (input && input.files[0]) targetFiles.push(input.files[0]);
  }

  if (targetFiles.length === 0) {
    showError("Please upload at least one comparison file.");
    return;
  }

  // ── Build FormData ───────────────────────────────────────────────────────

  const form = new FormData();
  form.append("source", sourceFileInput.files[0]);
  targetFiles.forEach(file => form.append("targets", file));

  // ── Send to Flask ────────────────────────────────────────────────────────

  showLoader();
  analyzeBtn.disabled = true;

  try {
    const response = await fetch("/analyze", { method: "POST", body: form });
    const data     = await response.json();

    if (!response.ok || data.error) {
      throw new Error(data.error || "Server returned an error.");
    }

    renderResults(data);

  } catch (err) {
    showError(err.message || "Something went wrong. Check the server is running.");
  } finally {
    hideLoader();
    analyzeBtn.disabled = false;
  }
}

// ── Render Results ────────────────────────────────────────────────────────

/**
 * Main rendering function.
 * @param {Object} data - JSON from /analyze endpoint
 */
function renderResults(data) {
  const results    = data.results    || [];
  const sourceText = data.source_text || "";
  const summary    = data.summary    || {};

  if (results.length === 0) {
    showError("No results returned. Check that your files have enough text.");
    return;
  }

  // ── Summary counts ───────────────────────────────────────────────────────
  document.getElementById("count-high").textContent = summary.high     ?? 0;
  document.getElementById("count-mod").textContent  = summary.moderate ?? 0;
  document.getElementById("count-low").textContent  = summary.low      ?? 0;

  // ── Bar Chart ────────────────────────────────────────────────────────────
  renderChart(results);

  // ── Per-file cards ───────────────────────────────────────────────────────
  resultsList.innerHTML = "";
  results.forEach((r, index) => renderResultCard(r, index + 1, sourceText));

  // Show the results section with animation
  resultsSection.style.display = "flex";
}

/** Draw (or redraw) the Chart.js bar chart. */
function renderChart(results) {
  if (chartInstance) {
    chartInstance.destroy();
    chartInstance = null;
  }

  const labels = results.map(r => r.file);
  const values = results.map(r => parseFloat(r.similarity.toFixed(1)));
  const colors = results.map(r => levelColor(r.level, 0.85));
  const borders = results.map(r => levelColor(r.level, 1));

  const ctx = document.getElementById("similarity-chart").getContext("2d");

  chartInstance = new Chart(ctx, {
    type: "bar",
    data: {
      labels,
      datasets: [{
        label: "Similarity %",
        data: values,
        backgroundColor: colors,
        borderColor: borders,
        borderWidth: 1.5,
        borderRadius: 5,
        borderSkipped: false,
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 600, easing: "easeOutQuart" },
      plugins: {
        legend: { display: false },
        tooltip: {
          callbacks: {
            label: ctx => ` ${ctx.parsed.y.toFixed(1)}% similarity`
          }
        }
      },
      scales: {
        x: {
          ticks: { color: "#7d8590", font: { size: 11 }, maxRotation: 30 },
          grid:  { color: "rgba(255,255,255,0.05)" }
        },
        y: {
          beginAtZero: true,
          max: 100,
          ticks: {
            color: "#7d8590",
            font: { size: 11 },
            callback: val => val + "%"
          },
          grid: { color: "rgba(255,255,255,0.05)" }
        }
      }
    }
  });
}

/** Build one expandable result card. */
function renderResultCard(result, rank, sourceText) {
  const pct   = parseFloat(result.similarity).toFixed(1);
  const level = result.level;
  const color = levelHex(level);

  const card  = document.createElement("div");
  card.className = "result-card";
  card.id        = `result-card-${rank}`;

  card.innerHTML = `
    <div class="result-header" role="button" aria-expanded="false" tabindex="0"
         aria-controls="diff-${rank}">
      <span class="result-rank">#${rank}</span>
      <span class="result-filename">${escapeHtml(result.file)}</span>
      <span class="result-pct" style="color:${color};">${pct}%</span>
      <span class="result-badge badge-${level}">${level}</span>
      <span class="result-chevron">▼</span>
    </div>

    <div class="progress-bar-wrap">
      <div class="progress-bar-fill"
           style="width:${pct}%; background:${color};"></div>
    </div>

    <div class="text-diff" id="diff-${rank}">
      <div class="diff-grid">
        <div class="diff-panel">
          <h4>Source File</h4>
          <div class="diff-text" id="src-text-${rank}"></div>
        </div>
        <div class="diff-panel">
          <h4>${escapeHtml(result.file)} — Matched text highlighted</h4>
          <div class="diff-text" id="tgt-text-${rank}"></div>
        </div>
      </div>
    </div>
  `;

  // Toggle open/close on click or Enter key
  const header = card.querySelector(".result-header");
  const toggle = () => {
    const isOpen = card.classList.toggle("open");
    header.setAttribute("aria-expanded", isOpen);

    // Populate text panels lazily (only when first opened)
    if (isOpen) {
      const srcEl = document.getElementById(`src-text-${rank}`);
      const tgtEl = document.getElementById(`tgt-text-${rank}`);
      if (!srcEl.dataset.loaded) {
        srcEl.innerHTML  = highlightMatches(sourceText, result.text || "");
        tgtEl.innerHTML  = highlightMatches(result.text || "", sourceText);
        srcEl.dataset.loaded = "1";
      }
    }
  };

  header.addEventListener("click", toggle);
  header.addEventListener("keydown", e => { if (e.key === "Enter") toggle(); });

  resultsList.appendChild(card);
}

// ── Text Highlighting ─────────────────────────────────────────────────────

/**
 * Highlight words from `reference` that also appear in `text`.
 * Only words longer than 4 chars are considered — avoids noisy short words.
 */
function highlightMatches(text, reference) {
  if (!text) return "<em style='color:#7d8590'>No text available.</em>";
  if (!reference) return escapeHtml(text);

  // Extract meaningful words from the reference
  const refWords = new Set(
    reference.split(/\W+/)
      .map(w => w.toLowerCase())
      .filter(w => w.length > 4)
  );

  // Replace word by word in the source text
  return escapeHtml(text).replace(/\b([a-zA-Z]{5,})\b/g, (match) => {
    return refWords.has(match.toLowerCase())
      ? `<mark>${match}</mark>`
      : match;
  });
}

// ── Utility Helpers ───────────────────────────────────────────────────────

function levelHex(level) {
  if (level === "high")     return "#f85149";
  if (level === "moderate") return "#e3b341";
  return "#3fb950";
}

function levelColor(level, alpha) {
  if (level === "high")     return `rgba(248,81,73,${alpha})`;
  if (level === "moderate") return `rgba(227,179,65,${alpha})`;
  return `rgba(63,185,80,${alpha})`;
}

/** Safely convert plain text to HTML (prevents XSS from filenames/content). */
function escapeHtml(str) {
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

// ── UI State Helpers ──────────────────────────────────────────────────────

function showError(msg) {
  errorBanner.textContent = "⚠️  " + msg;
  errorBanner.style.display = "block";
}

function hideError() {
  errorBanner.style.display = "none";
  errorBanner.textContent   = "";
}

function showLoader() {
  loader.style.display = "flex";
}

function hideLoader() {
  loader.style.display = "none";
}

function hideResults() {
  resultsSection.style.display = "none";
  resultsList.innerHTML = "";
}