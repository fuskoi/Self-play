const STORAGE_KEY = "private-links-extras";
const OVERRIDES_KEY = "private-links-overrides";
const DELETED_KEY = "private-links-deleted";
const AUTH_KEY = "private-links-auth";
const SORT_KEY = "private-links-sort";
const DIM_KEY = "private-links-bg-dim";
const BG_KEY = "private-links-bg-image";
const LOCAL_IMAGES_KEY = "private-links-local-images";
const PAGE_SIZE = 12;

const panLabel = {
  quark: "夸克网盘",
  baidu: "百度网盘",
  other: "其他链接",
};

function detectPan(url, fallback = "other") {
  const u = String(url || "").toLowerCase();
  if (/quark\.cn|pan\.quark/.test(u)) return "quark";
  if (/baidu\.com|yun\.baidu|pan\.baidu/.test(u)) return "baidu";
  return fallback === "quark" || fallback === "baidu" ? fallback : "other";
}

function normalizeLinks(item) {
  if (Array.isArray(item?.links) && item.links.length) {
    return item.links
      .map((l) => ({
        url: String(l.url || "").trim(),
        note: String(l.note || "").trim(),
      }))
      .filter((l) => l.url);
  }
  if (item?.url) return [{ url: String(item.url).trim(), note: "" }];
  return [];
}

function normalizeTags(tags) {
  if (Array.isArray(tags)) {
    return [...new Set(tags.map((t) => String(t).trim()).filter(Boolean))];
  }
  if (typeof tags === "string") {
    return [
      ...new Set(
        tags
          .split(/[,，、]/)
          .map((t) => t.trim())
          .filter(Boolean)
      ),
    ];
  }
  return [];
}

function createdAtOf(item) {
  if (typeof item?.createdAt === "number") return item.createdAt;
  const m = String(item?.id || "").match(/^local-(\d+)/);
  if (m) return Number(m[1]);
  return 0;
}

function normalizeItem(item) {
  return {
    ...item,
    links: normalizeLinks(item),
    tags: normalizeTags(item?.tags),
    pinned: Boolean(item?.pinned),
    createdAt: createdAtOf(item),
    url: undefined,
    pan: undefined,
  };
}

async function hashPassword(text) {
  const data = new TextEncoder().encode(text);
  const buf = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(buf)]
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
}
window.hashPassword = hashPassword;

function remoteConfig() {
  const r = window.SITE_CONFIG?.remote || {};
  const binId = String(r.binId || "").trim();
  const apiKey = String(r.apiKey || "").trim();
  return {
    enabled: Boolean(binId && apiKey),
    binId,
    apiKey,
  };
}

function loadExtras() {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY) || "[]").map(normalizeItem);
  } catch {
    return [];
  }
}

function saveExtras(list, opts = {}) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(list.map(normalizeItem)));
  if (!opts.silent) queueCloudSync();
}

function loadOverrides() {
  try {
    return JSON.parse(localStorage.getItem(OVERRIDES_KEY) || "{}");
  } catch {
    return {};
  }
}

function saveOverrides(map, opts = {}) {
  localStorage.setItem(OVERRIDES_KEY, JSON.stringify(map));
  if (!opts.silent) queueCloudSync();
}

function loadDeleted() {
  try {
    return new Set(JSON.parse(localStorage.getItem(DELETED_KEY) || "[]"));
  } catch {
    return new Set();
  }
}

function saveDeleted(set, opts = {}) {
  localStorage.setItem(DELETED_KEY, JSON.stringify([...set]));
  if (!opts.silent) queueCloudSync();
}

function loadLocalImages() {
  try {
    return JSON.parse(localStorage.getItem(LOCAL_IMAGES_KEY) || "{}");
  } catch {
    return {};
  }
}

function saveLocalImages(map) {
  localStorage.setItem(LOCAL_IMAGES_KEY, JSON.stringify(map));
}

function setLocalImage(id, image) {
  if (!id) return;
  const map = loadLocalImages();
  if (image && String(image).startsWith("data:")) {
    map[id] = image;
  } else {
    delete map[id];
  }
  saveLocalImages(map);
}

function isHeavyImage(image) {
  return typeof image === "string" && image.startsWith("data:");
}

function slimItemForCloud(item) {
  const next = normalizeItem(item);
  if (isHeavyImage(next.image)) next.image = "";
  return next;
}

function migrateHeavyImagesToLocal() {
  const images = loadLocalImages();
  let changed = false;

  const extras = loadExtras().map((item) => {
    if (!isHeavyImage(item.image)) return item;
    images[item.id] = item.image;
    changed = true;
    return normalizeItem({ ...item, image: "" });
  });

  const overrides = loadOverrides();
  Object.keys(overrides).forEach((id) => {
    const row = overrides[id];
    if (!isHeavyImage(row?.image)) return;
    images[id] = row.image;
    overrides[id] = { ...row, image: "" };
    changed = true;
  });

  if (!changed) return false;
  saveLocalImages(images);
  saveExtras(extras, { silent: true });
  saveOverrides(overrides, { silent: true });
  return true;
}

function cloudPayload() {
  const overrides = loadOverrides();
  const slimOverrides = {};
  Object.keys(overrides).forEach((id) => {
    slimOverrides[id] = slimItemForCloud(overrides[id]);
  });
  return {
    version: 1,
    extras: loadExtras().map(slimItemForCloud),
    overrides: slimOverrides,
    deleted: [...loadDeleted()],
  };
}

function applyCloudPayload(data, opts = {}) {
  // 大图只留在本机 LOCAL_IMAGES，不写回 extras/overrides，避免再次撑爆云端
  const extras = (Array.isArray(data?.extras) ? data.extras : []).map((item) =>
    slimItemForCloud(item)
  );
  const overrides =
    data?.overrides && typeof data.overrides === "object" ? data.overrides : {};
  const mergedOverrides = {};
  Object.keys(overrides).forEach((id) => {
    mergedOverrides[id] = slimItemForCloud({ id, ...overrides[id] });
  });
  const deleted = Array.isArray(data?.deleted) ? data.deleted : [];
  saveExtras(extras, { silent: true });
  saveOverrides(mergedOverrides, { silent: true });
  saveDeleted(new Set(deleted), { silent: true });
  if (!opts.silent) queueCloudSync();
}

function setSyncStatus(state, text) {
  const el = document.getElementById("sync-status");
  const tip = document.getElementById("remote-tip");
  if (!el) return;
  if (!text) {
    el.hidden = true;
    el.textContent = "";
    return;
  }
  el.hidden = false;
  el.dataset.state = state;
  el.textContent = text;
  if (tip) {
    tip.textContent = remoteConfig().enabled
      ? "已开启云端同步：任意电脑改完，其他电脑刷新后可见。免费额度约 100KB，封面请尽量用图片链接；本机上传的大图只留在本机。"
      : "未配置云端。请在 config.js 填写 remote.binId 和 remote.apiKey 后重新发布。";
  }
}

let syncTimer = null;
let syncing = false;

async function readJsonbinError(res) {
  try {
    const data = await res.json();
    return data.message || data.Message || res.statusText || String(res.status);
  } catch {
    return res.statusText || String(res.status);
  }
}

async function pullCloud() {
  const { enabled, binId, apiKey } = remoteConfig();
  if (!enabled) {
    setSyncStatus("off", "仅本机存储（未配置云端）");
    return false;
  }
  setSyncStatus("syncing", "正在从云端拉取…");
  const res = await fetch(`https://api.jsonbin.io/v3/b/${binId}/latest`, {
    headers: {
      "X-Master-Key": apiKey,
    },
  });
  if (!res.ok) {
    const msg = await readJsonbinError(res);
    if (res.status === 401) {
      throw new Error("密钥无效：请重新复制完整的 X-主钥匙");
    }
    throw new Error(friendlySyncError(msg));
  }
  const json = await res.json();
  const record = json.record || json;
  const localCount = allResources().length;
  const remoteCount =
    (Array.isArray(record.extras) ? record.extras.length : 0) +
    Object.keys(record.overrides || {}).length;
  // 云端空、本机有数据：先上传本机（已去掉本地大图）
  if (remoteCount === 0 && localCount > 0) {
    migrateHeavyImagesToLocal();
    await pushCloud();
    setSyncStatus("ok", "已把本机数据上传到云端");
    return true;
  }
  applyCloudPayload(record, { silent: true });
  setSyncStatus("ok", "已同步云端");
  return true;
}

function friendlySyncError(msg) {
  const text = String(msg || "");
  if (/100kb|100 KB|over 100/i.test(text)) {
    return "云端免费额度仅 100KB，已改为不同步本地大图。请再点「立即同步」或刷新";
  }
  return text;
}

async function pushCloud() {
  const { enabled, binId, apiKey } = remoteConfig();
  if (!enabled) return false;
  migrateHeavyImagesToLocal();
  const body = JSON.stringify(cloudPayload());
  if (body.length > 100 * 1024) {
    throw new Error(
      "同步内容仍超过 100KB，请把封面改成图片链接（不要上传大图），或删减部分卡片"
    );
  }
  const res = await fetch(`https://api.jsonbin.io/v3/b/${binId}`, {
    method: "PUT",
    headers: {
      "Content-Type": "application/json",
      "X-Master-Key": apiKey,
    },
    body,
  });
  if (!res.ok) {
    const msg = await readJsonbinError(res);
    if (res.status === 401) {
      throw new Error("密钥无效：请重新复制完整的 X-主钥匙");
    }
    throw new Error(friendlySyncError(msg));
  }
  return true;
}

function queueCloudSync() {
  const { enabled } = remoteConfig();
  if (!enabled) return;
  setSyncStatus("syncing", "正在同步到云端…");
  clearTimeout(syncTimer);
  syncTimer = setTimeout(async () => {
    if (syncing) return;
    syncing = true;
    try {
      await pushCloud();
      setSyncStatus("ok", "已同步到所有设备");
    } catch (err) {
      setSyncStatus("error", "同步失败：" + (err.message || "请检查网络"));
      console.error(err);
    } finally {
      syncing = false;
    }
  }, 450);
}

async function syncNow() {
  const { enabled } = remoteConfig();
  if (!enabled) {
    alert("还没配置云端。打开 config.js，按注释填写 JSONBin 的 binId 和 apiKey，再重新发布。");
    return;
  }
  try {
    setSyncStatus("syncing", "正在同步…");
    await pullCloud();
    render();
  } catch (err) {
    setSyncStatus("error", "同步失败");
    alert("同步失败：" + (err.message || "请稍后重试"));
  }
}

function isLocalId(id) {
  return String(id || "").startsWith("local-");
}

function withLocalImage(item) {
  const n = normalizeItem(item);
  const localImages = loadLocalImages();
  if ((!n.image || isHeavyImage(n.image)) && localImages[n.id]) {
    n.image = localImages[n.id];
  }
  return n;
}

function allResources() {
  const overrides = loadOverrides();
  const deleted = loadDeleted();
  const base = (Array.isArray(window.RESOURCES) ? window.RESOURCES : [])
    .filter((r) => !deleted.has(r.id))
    .map((r) => withLocalImage({ ...r, ...(overrides[r.id] || {}) }));
  return [...base, ...loadExtras().map(withLocalImage)];
}

function findById(id) {
  return allResources().find((x) => x.id === id);
}

function deleteItem(id) {
  setLocalImage(id, "");
  if (isLocalId(id)) {
    saveExtras(loadExtras().filter((x) => x.id !== id));
    return;
  }
  const deleted = loadDeleted();
  deleted.add(id);
  saveDeleted(deleted);
  const map = loadOverrides();
  if (map[id]) {
    delete map[id];
    saveOverrides(map);
  }
}

function patchItem(id, fields) {
  const current = findById(id);
  if (!current) return;
  const next = normalizeItem({ ...current, ...fields, id });
  if ("image" in fields) setLocalImage(id, next.image);
  const cloudImage = isHeavyImage(next.image) ? "" : next.image;

  if (isLocalId(id)) {
    const list = loadExtras();
    const idx = list.findIndex((x) => x.id === id);
    if (idx >= 0) {
      list[idx] = normalizeItem({ ...next, image: cloudImage });
      saveExtras(list);
    }
    return;
  }

  const map = loadOverrides();
  map[id] = {
    ...(map[id] || {}),
    title: next.title,
    image: cloudImage,
    links: next.links,
    category: next.category,
    note: next.note,
    tags: next.tags,
    pinned: next.pinned,
    createdAt: next.createdAt,
  };
  saveOverrides(map);
}

function isUnlocked() {
  return sessionStorage.getItem(AUTH_KEY) === "1";
}

function setUnlocked(ok) {
  if (ok) sessionStorage.setItem(AUTH_KEY, "1");
  else sessionStorage.removeItem(AUTH_KEY);
}

const gate = document.getElementById("gate");
const app = document.getElementById("app");
const grid = document.getElementById("grid");
const empty = document.getElementById("empty");
const countEl = document.getElementById("count");
const searchInput = document.getElementById("search");
const editor = document.getElementById("editor");
const manager = document.getElementById("manager");
const managerList = document.getElementById("manager-list");
const managerEmpty = document.getElementById("manager-empty");
const editorForm = document.getElementById("editor-form");
const linksEditor = document.getElementById("links-editor");
const imagePreview = document.getElementById("image-preview");
const imagePreviewImg = document.getElementById("image-preview-img");
const editImageData = document.getElementById("edit-image-data");
const editImageUrl = document.getElementById("edit-image");
const editImageFile = document.getElementById("edit-image-file");
const tagFilters = document.getElementById("tag-filters");
const bgDimInput = document.getElementById("bg-dim");
const randomDialog = document.getElementById("random-dialog");
const randomCard = document.getElementById("random-card");
const bgDialog = document.getElementById("bg-dialog");
const batchCountEl = document.getElementById("batch-count");
const batchSelectAll = document.getElementById("batch-select-all");
const batchBar = document.getElementById("batch-bar");
const btnBatchToggle = document.getElementById("btn-batch-toggle");

let activeCat = "全部";
let activeTag = "";
let activeSort = localStorage.getItem(SORT_KEY) === "name" ? "name" : "added";
let currentPage = 1;
let batchMode = false;
const selectedIds = new Set();

const pager = document.getElementById("pager");
const pageInfo = document.getElementById("page-info");
const btnFirst = document.getElementById("btn-first");
const btnPrev = document.getElementById("btn-prev");
const btnNext = document.getElementById("btn-next");
const btnLast = document.getElementById("btn-last");
let totalPagesCache = 1;

function applyConfig() {
  const cfg = window.SITE_CONFIG || {};
  const title = cfg.title || "我的资源库";
  document.title = title;
  document.getElementById("gate-brand").textContent = title;
  document.getElementById("brand").textContent = title;
  document.getElementById("tagline").textContent =
    cfg.subtitle || "私人链接 · 仅自己可见";
}

function applyBgDim(value) {
  const v = Math.min(85, Math.max(30, Number(value) || 62));
  document.documentElement.style.setProperty("--bg-dim", String(v / 100));
  bgDimInput.value = String(v);
  localStorage.setItem(DIM_KEY, String(v));
}

function applyBgImage(dataUrl) {
  const nameEl = document.getElementById("bg-file-name");
  if (dataUrl) {
    document.documentElement.style.setProperty(
      "--bg-image",
      `url(${JSON.stringify(dataUrl)})`
    );
    localStorage.setItem(BG_KEY, dataUrl);
    if (nameEl) nameEl.textContent = "已使用本机自定义背景";
  } else {
    document.documentElement.style.removeProperty("--bg-image");
    localStorage.removeItem(BG_KEY);
    if (nameEl) nameEl.textContent = "使用网站默认";
  }
}

function compressBackground(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error("读取图片失败"));
    reader.onload = () => {
      const img = new Image();
      img.onerror = () => reject(new Error("图片无法解析"));
      img.onload = () => {
        const maxW = 1600;
        const scale = Math.min(1, maxW / img.width);
        const canvas = document.createElement("canvas");
        canvas.width = Math.round(img.width * scale);
        canvas.height = Math.round(img.height * scale);
        const ctx = canvas.getContext("2d");
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
        resolve(canvas.toDataURL("image/jpeg", 0.72));
      };
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

async function showApp() {
  document.body.classList.remove("is-locked");
  document.body.classList.add("is-unlocked");
  gate.hidden = true;
  app.hidden = false;
  migrateHeavyImagesToLocal();
  if (remoteConfig().enabled) {
    try {
      await pullCloud();
    } catch (err) {
      console.error(err);
      // 若因体积失败，清掉大图后再强制推一次瘦身后的数据
      try {
        migrateHeavyImagesToLocal();
        await pushCloud();
        setSyncStatus("ok", "已压缩并重新同步到云端");
      } catch (err2) {
        setSyncStatus("error", "云端同步失败：" + (err2.message || err.message || ""));
      }
    }
  } else {
    setSyncStatus("off", "仅本机存储（未配置云端）");
  }
  const tip = document.getElementById("remote-tip");
  if (tip) {
    tip.textContent = remoteConfig().enabled
      ? "已开启云端同步：任意电脑改完，其他电脑刷新后可见。免费额度约 100KB，封面请尽量用图片链接；本机上传的大图只留在本机。"
      : "未配置云端。请在 config.js 填写 remote.binId 和 remote.apiKey 后重新发布。";
  }
  render();
}

function showGate() {
  document.body.classList.add("is-locked");
  document.body.classList.remove("is-unlocked");
  app.hidden = true;
  gate.hidden = false;
  grid.innerHTML = "";
  countEl.textContent = "";
  if (editor.open) editor.close();
  if (manager.open) manager.close();
  if (randomDialog.open) randomDialog.close();
  if (bgDialog.open) bgDialog.close();
  document.getElementById("password").value = "";
  document.getElementById("gate-error").hidden = true;
}

function filtered() {
  const q = searchInput.value.trim().toLowerCase();
  const list = allResources().filter((item) => {
    if (activeCat === "收藏") {
      if (!item.pinned) return false;
    } else if (activeCat !== "全部" && item.category !== activeCat) {
      return false;
    }
    if (activeTag && !item.tags.includes(activeTag)) return false;
    if (!q) return true;
    const pans = normalizeLinks(item)
      .map((l) => panLabel[detectPan(l.url)])
      .join(" ");
    const blob = [item.title, item.note, item.category, pans, ...(item.tags || [])]
      .filter(Boolean)
      .join(" ")
      .toLowerCase();
    return blob.includes(q);
  });

  list.sort((a, b) => {
    if (a.pinned !== b.pinned) return a.pinned ? -1 : 1;
    if (activeSort === "name") {
      return String(a.title || "").localeCompare(String(b.title || ""), "zh-CN");
    }
    return createdAtOf(a) - createdAtOf(b);
  });
  return list;
}

function syncSortButtons() {
  document.querySelectorAll(".sort-option").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.sort === activeSort);
  });
}

function escapeHtml(str) {
  return String(str ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function escapeAttr(str) {
  return escapeHtml(str).replaceAll("'", "&#39;");
}

function coverHtml(item) {
  const src = item.image;
  if (src) {
    return `<div class="card-cover"><img src="${escapeAttr(src)}" alt="" loading="lazy" decoding="async" /></div>`;
  }
  const letter = (item.title || "?").trim().charAt(0).toUpperCase();
  return `<div class="card-cover"><div class="card-cover-fallback">${escapeHtml(letter)}</div></div>`;
}

function tagsHtml(item) {
  if (!item.tags?.length) return "";
  return `<div class="card-tags">${item.tags
    .map((t) => `<span class="tag-chip">${escapeHtml(t)}</span>`)
    .join("")}</div>`;
}

function linksButtonsHtml(item) {
  const links = normalizeLinks(item);
  if (!links.length) return `<span class="pan-tag other">暂无链接</span>`;
  return links
    .map((link) => {
      const pan = detectPan(link.url);
      const label = panLabel[pan] || "其他链接";
      const title = link.note ? ` title="${escapeAttr(link.note)}"` : "";
      return `<a class="pan-tag ${escapeHtml(pan)}" href="${escapeAttr(link.url)}" target="_blank" rel="noopener noreferrer"${title}>${escapeHtml(label)}</a>`;
    })
    .join("");
}

function renderPager(total) {
  const totalPages = Math.max(1, Math.ceil(total / PAGE_SIZE));
  totalPagesCache = totalPages;
  if (currentPage > totalPages) currentPage = totalPages;
  if (currentPage < 1) currentPage = 1;
  if (total <= PAGE_SIZE) {
    pager.hidden = true;
    return { start: 0, end: total, totalPages: 1 };
  }
  pager.hidden = false;
  pageInfo.textContent = `第 ${currentPage} / ${totalPages} 页`;
  const atFirst = currentPage <= 1;
  const atLast = currentPage >= totalPages;
  btnFirst.disabled = atFirst;
  btnPrev.disabled = atFirst;
  btnNext.disabled = atLast;
  btnLast.disabled = atLast;
  const start = (currentPage - 1) * PAGE_SIZE;
  return { start, end: start + PAGE_SIZE, totalPages };
}

function goToPage(page) {
  currentPage = page;
  render();
  window.scrollTo({ top: 0, behavior: "smooth" });
}

function renderTagFilters() {
  const tags = [
    ...new Set(allResources().flatMap((r) => r.tags || [])),
  ].sort((a, b) => a.localeCompare(b, "zh-CN"));
  if (!tags.length) {
    tagFilters.hidden = true;
    tagFilters.innerHTML = "";
    activeTag = "";
    return;
  }
  tagFilters.hidden = false;
  tagFilters.innerHTML =
    `<button type="button" class="tag-filter ${activeTag ? "" : "active"}" data-tag="">全部标签</button>` +
    tags
      .map(
        (t) =>
          `<button type="button" class="tag-filter ${
            activeTag === t ? "active" : ""
          }" data-tag="${escapeAttr(t)}">${escapeHtml(t)}</button>`
      )
      .join("");
}

function render() {
  const list = filtered();
  const { start, end } = renderPager(list.length);
  const pageList = list.slice(start, end);

  countEl.textContent =
    list.length > PAGE_SIZE
      ? `共 ${list.length} 个资源 · 本页 ${pageList.length} 个`
      : `共 ${list.length} 个资源`;
  empty.hidden = list.length > 0;
  grid.innerHTML = pageList
    .map((item, i) => {
      return `
      <article class="card ${item.pinned ? "is-pinned" : ""}" style="animation-delay:${Math.min(i, 10) * 40}ms">
        <button type="button" class="pin-btn ${item.pinned ? "on" : ""}" data-pin="${escapeAttr(item.id)}" title="${item.pinned ? "取消收藏" : "收藏置顶"}">${item.pinned ? "★" : "☆"}</button>
        ${coverHtml(item)}
        <div class="card-body">
          <div class="card-top">
            <span class="cat-pill">${escapeHtml(item.category)}</span>
          </div>
          <h3>${escapeHtml(item.title)}</h3>
          ${tagsHtml(item)}
          ${item.note ? `<p class="note">备注：${escapeHtml(item.note)}</p>` : ""}
          <div class="card-actions">
            ${linksButtonsHtml(item)}
          </div>
        </div>
      </article>`;
    })
    .join("");

  renderTagFilters();
  if (manager.open) renderManager();
}

function updateBatchCount() {
  batchCountEl.textContent = `已选 ${selectedIds.size}`;
  const boxes = [...managerList.querySelectorAll(".batch-item")];
  batchSelectAll.checked =
    boxes.length > 0 && boxes.every((b) => selectedIds.has(b.value));
}

function syncBatchModeUi() {
  batchBar.hidden = !batchMode;
  manager.classList.toggle("is-batch", batchMode);
  btnBatchToggle.textContent = batchMode ? "退出批量" : "批量操作";
  btnBatchToggle.classList.toggle("primary", batchMode);
}

function renderManager() {
  const list = allResources();
  managerEmpty.hidden = list.length > 0;
  syncBatchModeUi();
  managerList.innerHTML = list
    .map((item) => {
      const links = normalizeLinks(item);
      const panText = links.length
        ? links.map((l) => panLabel[detectPan(l.url)]).join(" / ")
        : "无链接";
      const tagText = item.tags?.length ? ` · ${item.tags.join(" / ")}` : "";
      const thumb = item.image
        ? `<img class="manager-thumb" src="${escapeAttr(item.image)}" alt="" />`
        : `<div class="manager-thumb fallback">${escapeHtml((item.title || "?").charAt(0))}</div>`;
      const checked = selectedIds.has(item.id) ? "checked" : "";
      const pick = batchMode
        ? `<label class="batch-pick"><input class="batch-item" type="checkbox" value="${escapeAttr(item.id)}" ${checked} /></label>`
        : "";
      return `
      <div class="manager-row">
        ${pick}
        ${thumb}
        <div class="manager-meta">
          <strong>${item.pinned ? "★ " : ""}${escapeHtml(item.title)}</strong>
          <span>${escapeHtml(item.category)} · ${escapeHtml(panText)}${escapeHtml(tagText)}</span>
        </div>
        <div class="manager-ops">
          <button type="button" data-edit="${escapeAttr(item.id)}">修改</button>
          <button type="button" class="danger" data-del="${escapeAttr(item.id)}">删除</button>
        </div>
      </div>`;
    })
    .join("");
  updateBatchCount();
}

function openManager() {
  batchMode = false;
  selectedIds.clear();
  batchSelectAll.checked = false;
  document.getElementById("batch-category").value = "";
  renderManager();
  manager.showModal();
}

function showPreview(src) {
  if (!src) {
    imagePreview.hidden = true;
    imagePreviewImg.removeAttribute("src");
    return;
  }
  imagePreviewImg.src = src;
  imagePreview.hidden = false;
}

function setFileNameLabel(name) {
  const el = document.getElementById("edit-image-file-name");
  if (el) el.textContent = name || "未选择文件";
}

function clearImageFields() {
  editImageData.value = "";
  editImageUrl.value = "";
  editImageFile.value = "";
  setFileNameLabel("");
  showPreview("");
}

function setImageFields(image) {
  clearImageFields();
  if (!image) return;
  if (image.startsWith("data:")) {
    editImageData.value = image;
    showPreview(image);
  } else {
    editImageUrl.value = image;
    showPreview(image);
  }
}

function compressImage(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error("读取图片失败"));
    reader.onload = () => {
      const img = new Image();
      img.onerror = () => reject(new Error("图片无法解析"));
      img.onload = () => {
        const maxW = 640;
        const scale = Math.min(1, maxW / img.width);
        const canvas = document.createElement("canvas");
        canvas.width = Math.round(img.width * scale);
        canvas.height = Math.round(img.height * scale);
        const ctx = canvas.getContext("2d");
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
        resolve(canvas.toDataURL("image/jpeg", 0.62));
      };
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

function updateLinkRowLabel(row) {
  const input = row.querySelector(".link-url");
  const label = row.querySelector(".link-pan-label");
  const pan = detectPan(input.value);
  label.textContent = panLabel[pan];
  label.className = "link-pan-label " + pan;
}

function createLinkRow(link = { url: "", note: "" }) {
  const row = document.createElement("div");
  row.className = "link-row";
  const pan = detectPan(link.url);
  row.innerHTML = `
    <div class="link-row-top">
      <strong class="link-pan-label ${pan}">${panLabel[pan]}</strong>
      <button type="button" class="btn-remove-link">删除</button>
    </div>
    <input class="link-url" type="url" required placeholder="https:// 网盘分享链接" value="${escapeAttr(link.url || "")}" />
    <input class="link-note" type="text" maxlength="40" placeholder="这条链接的提取码（可选）" value="${escapeAttr(link.note || "")}" />
  `;
  row.querySelector(".link-url").addEventListener("input", () => updateLinkRowLabel(row));
  row.querySelector(".btn-remove-link").addEventListener("click", () => {
    const rows = linksEditor.querySelectorAll(".link-row");
    if (rows.length <= 1) {
      row.querySelector(".link-url").value = "";
      row.querySelector(".link-note").value = "";
      updateLinkRowLabel(row);
      return;
    }
    row.remove();
    refreshLinkRemoveState();
  });
  return row;
}

function refreshLinkRemoveState() {
  const rows = [...linksEditor.querySelectorAll(".link-row")];
  rows.forEach((row) => {
    row.querySelector(".btn-remove-link").textContent =
      rows.length <= 1 ? "清空" : "删除";
  });
}

function setLinkRows(links) {
  linksEditor.innerHTML = "";
  const list = links?.length ? links : [{ url: "", note: "" }];
  list.forEach((link) => linksEditor.appendChild(createLinkRow(link)));
  refreshLinkRemoveState();
}

function readLinkRows() {
  return [...linksEditor.querySelectorAll(".link-row")]
    .map((row) => ({
      url: row.querySelector(".link-url").value.trim(),
      note: row.querySelector(".link-note").value.trim(),
    }))
    .filter((l) => l.url);
}

function setCategory(value) {
  const cat = value || "办公";
  document.getElementById("edit-category").value = cat;
  document.querySelectorAll(".cat-option").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.value === cat);
  });
}

function openEditor(item) {
  editorForm.reset();
  clearImageFields();

  if (item) {
    document.getElementById("editor-title").textContent = "修改资源";
    document.getElementById("edit-id").value = item.id;
    document.getElementById("edit-title").value = item.title || "";
    document.getElementById("edit-note").value = item.note || "";
    document.getElementById("edit-tags").value = (item.tags || []).join(", ");
    document.getElementById("edit-pin-check").checked = Boolean(item.pinned);
    setImageFields(item.image || "");
    setLinkRows(normalizeLinks(item));
    setCategory(item.category || "办公");
  } else {
    document.getElementById("editor-title").textContent = "添加资源";
    document.getElementById("edit-id").value = "";
    document.getElementById("edit-tags").value = "";
    document.getElementById("edit-pin-check").checked = false;
    setLinkRows([{ url: "", note: "" }]);
    setCategory("办公");
  }

  editor.showModal();
}

function saveItem(payload) {
  const rawImage = payload.image || "";
  const cloudImage = isHeavyImage(rawImage) ? "" : rawImage;

  if (payload.id && isLocalId(payload.id)) {
    setLocalImage(payload.id, isHeavyImage(rawImage) ? rawImage : "");
    const list = loadExtras();
    const idx = list.findIndex((x) => x.id === payload.id);
    if (idx >= 0) {
      list[idx] = normalizeItem({
        ...list[idx],
        ...payload,
        id: payload.id,
        image: cloudImage,
        createdAt: list[idx].createdAt || createdAtOf(list[idx]),
      });
      saveExtras(list);
      return;
    }
  }

  if (payload.id && !isLocalId(payload.id)) {
    setLocalImage(payload.id, isHeavyImage(rawImage) ? rawImage : "");
    const map = loadOverrides();
    map[payload.id] = {
      title: payload.title,
      image: cloudImage,
      links: payload.links,
      category: payload.category,
      note: payload.note,
      tags: payload.tags,
      pinned: payload.pinned,
      createdAt: payload.createdAt || createdAtOf({ id: payload.id }),
    };
    saveOverrides(map);
    return;
  }

  const now = Date.now();
  const newId = "local-" + now;
  setLocalImage(newId, isHeavyImage(rawImage) ? rawImage : "");
  saveExtras([
    ...loadExtras(),
    normalizeItem({
      ...payload,
      id: newId,
      createdAt: now,
      image: cloudImage,
    }),
  ]);
}

function showRandom() {
  const list = filtered();
  if (!list.length) {
    alert("当前筛选下没有可随机的资源");
    return;
  }
  const item = list[Math.floor(Math.random() * list.length)];
  randomCard.innerHTML = `
    <article class="card random-show">
      ${coverHtml(item)}
      <div class="card-body">
        <div class="card-top">
          <span class="cat-pill">${escapeHtml(item.category)}</span>
          ${item.pinned ? '<span class="cat-pill">收藏</span>' : ""}
        </div>
        <h3>${escapeHtml(item.title)}</h3>
        ${tagsHtml(item)}
        ${item.note ? `<p class="note">备注：${escapeHtml(item.note)}</p>` : ""}
        <div class="card-actions">${linksButtonsHtml(item)}</div>
      </div>
    </article>`;
  if (!randomDialog.open) randomDialog.showModal();
}

function buildBackup() {
  return {
    version: 1,
    app: "private-links",
    exportedAt: new Date().toISOString(),
    extras: loadExtras(),
    overrides: loadOverrides(),
    deleted: [...loadDeleted()],
  };
}

function contentKey(item) {
  const links = normalizeLinks(item)
    .map((l) => l.url)
    .sort()
    .join("|");
  return `${String(item.title || "").trim()}::${links}`;
}

function parseBackup(data) {
  if (!data || data.app !== "private-links") {
    throw new Error("不是本站的备份文件");
  }
  return {
    extras: Array.isArray(data.extras) ? data.extras.map(normalizeItem) : [],
    overrides:
      data.overrides && typeof data.overrides === "object" ? data.overrides : {},
    deleted: Array.isArray(data.deleted) ? data.deleted : [],
  };
}

function applyBackupReplace(data) {
  const { extras, overrides, deleted } = parseBackup(data);
  saveExtras(extras);
  saveOverrides(overrides);
  saveDeleted(new Set(deleted));
  return extras.length;
}

function applyBackupMerge(data) {
  const { extras: incoming, overrides: incomingOverrides } = parseBackup(data);
  const current = loadExtras();
  const existingIds = new Set(allResources().map((r) => r.id));
  const existingContent = new Set(allResources().map(contentKey));
  let added = 0;
  for (const raw of incoming) {
    const item = normalizeItem(raw);
    if (existingIds.has(item.id)) continue;
    if (existingContent.has(contentKey(item))) continue;
    let id = item.id;
    if (!id || existingIds.has(id)) id = "local-" + Date.now() + "-" + added;
    const next = normalizeItem({
      ...item,
      id,
      createdAt: item.createdAt || Date.now() + added,
    });
    current.push(next);
    existingIds.add(next.id);
    existingContent.add(contentKey(next));
    added += 1;
  }
  saveExtras(current);
  const map = loadOverrides();
  for (const [id, val] of Object.entries(incomingOverrides)) {
    if (map[id]) continue;
    map[id] = val;
  }
  saveOverrides(map);
  return added;
}

function exportBackup() {
  const blob = new Blob([JSON.stringify(buildBackup(), null, 2)], {
    type: "application/json",
  });
  const stamp = new Date().toISOString().slice(0, 10);
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `资源库备份-${stamp}.json`;
  a.click();
  URL.revokeObjectURL(a.href);
}

document.getElementById("gate-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const input = document.getElementById("password").value;
  const err = document.getElementById("gate-error");
  const expected = window.SITE_CONFIG?.passwordHash || "";
  const actual = await hashPassword(input);
  if (actual === expected) {
    setUnlocked(true);
    err.hidden = true;
    showApp();
  } else {
    err.hidden = false;
  }
});

document.querySelectorAll(".tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((t) => t.classList.remove("active"));
    tab.classList.add("active");
    activeCat = tab.dataset.cat;
    currentPage = 1;
    render();
  });
});

tagFilters.addEventListener("click", (e) => {
  const btn = e.target.closest(".tag-filter");
  if (!btn) return;
  activeTag = btn.dataset.tag || "";
  currentPage = 1;
  render();
});

searchInput.addEventListener("input", () => {
  currentPage = 1;
  render();
});

document.querySelector(".sort-picker").addEventListener("click", (e) => {
  const btn = e.target.closest(".sort-option");
  if (!btn) return;
  activeSort = btn.dataset.sort === "name" ? "name" : "added";
  localStorage.setItem(SORT_KEY, activeSort);
  syncSortButtons();
  currentPage = 1;
  render();
});

bgDimInput.addEventListener("input", () => applyBgDim(bgDimInput.value));

document.getElementById("btn-bg").addEventListener("click", () => {
  bgDialog.showModal();
});

document.getElementById("btn-bg-close").addEventListener("click", () => {
  bgDialog.close();
});

document.getElementById("bg-file").addEventListener("change", async (e) => {
  const file = e.target.files?.[0];
  if (!file) return;
  try {
    const dataUrl = await compressBackground(file);
    applyBgImage(dataUrl);
  } catch {
    alert("背景设置失败，请换一张小一点的图片试试");
  } finally {
    e.target.value = "";
  }
});

document.getElementById("btn-bg-reset").addEventListener("click", () => {
  applyBgImage("");
  document.getElementById("bg-file").value = "";
});

btnFirst.addEventListener("click", () => {
  if (currentPage > 1) goToPage(1);
});

btnPrev.addEventListener("click", () => {
  if (currentPage > 1) goToPage(currentPage - 1);
});

btnNext.addEventListener("click", () => {
  if (currentPage < totalPagesCache) goToPage(currentPage + 1);
});

btnLast.addEventListener("click", () => {
  if (currentPage < totalPagesCache) goToPage(totalPagesCache);
});

grid.addEventListener("click", (e) => {
  const pinBtn = e.target.closest("[data-pin]");
  if (!pinBtn) return;
  const id = pinBtn.getAttribute("data-pin");
  const item = findById(id);
  if (!item) return;
  patchItem(id, { pinned: !item.pinned });
  render();
});

managerList.addEventListener("click", (e) => {
  const editBtn = e.target.closest("[data-edit]");
  if (editBtn) {
    const item = findById(editBtn.getAttribute("data-edit"));
    if (item) openEditor(item);
    return;
  }
  const delBtn = e.target.closest("[data-del]");
  if (delBtn) {
    const id = delBtn.getAttribute("data-del");
    if (confirm("确定删除这个资源？")) {
      deleteItem(id);
      selectedIds.delete(id);
      render();
    }
  }
});

managerList.addEventListener("change", (e) => {
  const box = e.target.closest(".batch-item");
  if (!box) return;
  if (box.checked) selectedIds.add(box.value);
  else selectedIds.delete(box.value);
  updateBatchCount();
});

batchSelectAll.addEventListener("change", () => {
  const boxes = [...managerList.querySelectorAll(".batch-item")];
  if (batchSelectAll.checked) {
    boxes.forEach((b) => {
      b.checked = true;
      selectedIds.add(b.value);
    });
  } else {
    boxes.forEach((b) => {
      b.checked = false;
      selectedIds.delete(b.value);
    });
  }
  updateBatchCount();
});

btnBatchToggle.addEventListener("click", () => {
  batchMode = !batchMode;
  if (!batchMode) {
    selectedIds.clear();
    batchSelectAll.checked = false;
  }
  renderManager();
});

document.getElementById("btn-batch-category").addEventListener("click", () => {
  const cat = document.getElementById("batch-category").value;
  if (!cat) {
    alert("请先选择要改成的分类");
    return;
  }
  if (!selectedIds.size) {
    alert("请先勾选资源");
    return;
  }
  [...selectedIds].forEach((id) => patchItem(id, { category: cat }));
  render();
  alert(`已将 ${selectedIds.size} 个资源改为「${cat}」`);
});

document.getElementById("btn-batch-pin").addEventListener("click", () => {
  if (!selectedIds.size) return alert("请先勾选资源");
  [...selectedIds].forEach((id) => patchItem(id, { pinned: true }));
  render();
});

document.getElementById("btn-batch-unpin").addEventListener("click", () => {
  if (!selectedIds.size) return alert("请先勾选资源");
  [...selectedIds].forEach((id) => patchItem(id, { pinned: false }));
  render();
});

document.getElementById("btn-batch-delete").addEventListener("click", () => {
  if (!selectedIds.size) return alert("请先勾选资源");
  if (!confirm(`确定删除选中的 ${selectedIds.size} 个资源？`)) return;
  [...selectedIds].forEach((id) => deleteItem(id));
  selectedIds.clear();
  render();
});

document.getElementById("btn-lock").addEventListener("click", () => {
  setUnlocked(false);
  showGate();
});

document.getElementById("btn-add").addEventListener("click", () => openEditor(null));
document.getElementById("btn-manage").addEventListener("click", () => openManager());
document.getElementById("btn-manager-close").addEventListener("click", () => manager.close());
document.getElementById("btn-sync-now").addEventListener("click", () => syncNow());
document.getElementById("btn-random").addEventListener("click", () => showRandom());
document.getElementById("btn-random-again").addEventListener("click", () => showRandom());
document.getElementById("btn-random-close").addEventListener("click", () => randomDialog.close());

const importFile = document.getElementById("import-file");
let importMode = "merge";

document.getElementById("btn-export").addEventListener("click", () => exportBackup());
document.getElementById("btn-import").addEventListener("click", () => {
  importMode = "merge";
  importFile.value = "";
  importFile.click();
});
document.getElementById("btn-import-replace").addEventListener("click", () => {
  importMode = "replace";
  importFile.value = "";
  importFile.click();
});

importFile.addEventListener("change", async () => {
  const file = importFile.files?.[0];
  if (!file) return;
  try {
    const text = await file.text();
    const data = JSON.parse(text);
    if (importMode === "replace") {
      if (!confirm("覆盖导入会替换当前浏览器里的全部卡片数据，确定继续？")) return;
      const n = applyBackupReplace(data);
      selectedIds.clear();
      render();
      alert(`覆盖导入完成，当前备份内卡片 ${n} 个`);
      return;
    }
    if (!confirm("合并导入：只添加目前没有的卡片，已有的不会改动。确定继续？")) return;
    const added = applyBackupMerge(data);
    render();
    alert(added > 0 ? `合并完成，新增 ${added} 个卡片` : "没有可新增的卡片（都已存在）");
  } catch (err) {
    alert(err?.message || "导入失败，请确认是本站导出的 JSON 备份");
  }
});

document.getElementById("btn-cancel").addEventListener("click", () => editor.close());
document.getElementById("btn-add-link").addEventListener("click", () => {
  linksEditor.appendChild(createLinkRow());
  refreshLinkRemoveState();
});
document.querySelector(".cat-picker").addEventListener("click", (e) => {
  const btn = e.target.closest(".cat-option");
  if (!btn) return;
  setCategory(btn.dataset.value);
});
document.getElementById("btn-clear-image").addEventListener("click", () => clearImageFields());

editImageUrl.addEventListener("input", () => {
  const url = editImageUrl.value.trim();
  if (url) {
    editImageData.value = "";
    editImageFile.value = "";
    showPreview(url);
  } else if (!editImageData.value) {
    showPreview("");
  }
});

editImageFile.addEventListener("change", async () => {
  const file = editImageFile.files?.[0];
  if (!file) {
    setFileNameLabel("");
    return;
  }
  setFileNameLabel(file.name);
  try {
    const dataUrl = await compressImage(file);
    editImageData.value = dataUrl;
    editImageUrl.value = "";
    showPreview(dataUrl);
  } catch {
    alert("图片处理失败，请换一张试试");
  }
});

editorForm.addEventListener("submit", (e) => {
  e.preventDefault();
  const links = readLinkRows();
  if (!links.length) {
    alert("请至少填写一条网盘链接");
    return;
  }
  const image = editImageData.value.trim() || editImageUrl.value.trim();
  saveItem({
    id: document.getElementById("edit-id").value.trim(),
    title: document.getElementById("edit-title").value.trim(),
    image,
    links,
    category: document.getElementById("edit-category").value,
    note: document.getElementById("edit-note").value.trim(),
    tags: normalizeTags(document.getElementById("edit-tags").value),
    pinned: document.getElementById("edit-pin-check").checked,
  });
  editor.close();
  render();
});

applyConfig();
syncSortButtons();
applyBgDim(localStorage.getItem(DIM_KEY) || "62");
applyBgImage(localStorage.getItem(BG_KEY) || "");
document.body.classList.add("is-locked");
if (isUnlocked()) showApp();
else {
  setSyncStatus(
    remoteConfig().enabled ? "ok" : "off",
    remoteConfig().enabled ? "" : ""
  );
  showGate();
}
