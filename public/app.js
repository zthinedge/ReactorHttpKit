const state = {
  user: JSON.parse(localStorage.getItem("rhk_user") || "null"),
  products: [],
  orders: []
};

const $ = (id) => document.getElementById(id);

function money(cents) {
  return `¥${(cents / 100).toFixed(2)}`;
}

function toast(message) {
  const el = $("toast");
  el.textContent = message;
  el.classList.add("show");
  clearTimeout(window.__toastTimer);
  window.__toastTimer = setTimeout(() => el.classList.remove("show"), 2400);
}

async function api(path, options = {}) {
  const res = await fetch(path, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });
  const data = await res.json();
  if (!data.ok) {
    throw new Error(data.error || "请求失败");
  }
  return data;
}

function renderUser() {
  $("currentUser").textContent = state.user ? `当前：${state.user.username}` : "未登录";
}

function renderProducts() {
  $("products").innerHTML = state.products.map((item) => `
    <article class="product">
      <h3>${item.name}</h3>
      <p>${item.description}</p>
      <div class="row">
        <span class="price">${money(item.priceCents)}</span>
        <span class="stock">库存 ${item.stock}</span>
      </div>
      <div class="row buy">
        <input id="qty-${item.id}" type="number" min="1" max="${item.stock}" value="1">
        <button ${item.stock <= 0 ? "disabled" : ""} onclick="createOrder(${item.id})">下单</button>
      </div>
    </article>
  `).join("");
}

function renderOrders() {
  $("orderCount").textContent = `${state.orders.length} 条`;
  if (state.orders.length === 0) {
    $("orders").innerHTML = `<p class="hint">暂无订单</p>`;
    return;
  }

  $("orders").innerHTML = state.orders.map((order) => `
    <article class="order">
      <div class="row">
        <h3>#${order.id} ${order.productName}</h3>
        <span class="status ${order.status === "CANCELED" ? "canceled" : ""}">${order.status}</span>
      </div>
      <p>${order.createdAt}，数量 ${order.quantity}，合计 ${money(order.totalCents)}</p>
      <button class="danger" ${order.status !== "CREATED" ? "disabled" : ""} onclick="cancelOrder(${order.id})">取消订单</button>
    </article>
  `).join("");
}

function renderSummary(summary) {
  const items = [
    ["users", "用户"],
    ["products", "商品"],
    ["orders", "订单"],
    ["stock", "总库存"]
  ];
  $("metrics").innerHTML = items.map(([key, label]) => `
    <div class="metric"><strong>${summary[key] ?? 0}</strong><span>${label}</span></div>
  `).join("");
}

async function refresh() {
  const [products, summary] = await Promise.all([
    api("/api/products"),
    api("/api/admin/summary")
  ]);
  state.products = products.products;
  renderProducts();
  renderSummary(summary.summary);
  if (state.user) {
    const orders = await api(`/api/orders?userId=${state.user.id}`);
    state.orders = orders.orders;
  } else {
    state.orders = [];
  }
  renderOrders();
}

async function login() {
  const username = $("username").value.trim();
  const password = $("password").value;
  const data = await api("/api/login", {
    method: "POST",
    body: JSON.stringify({ username, password })
  });
  state.user = data.user;
  localStorage.setItem("rhk_user", JSON.stringify(state.user));
  renderUser();
  await refresh();
  toast("登录成功");
}

async function register() {
  const username = $("username").value.trim();
  const password = $("password").value;
  const data = await api("/api/register", {
    method: "POST",
    body: JSON.stringify({ username, password })
  });
  state.user = data.user;
  localStorage.setItem("rhk_user", JSON.stringify(state.user));
  renderUser();
  await refresh();
  toast("注册成功");
}

async function createOrder(productId) {
  if (!state.user) {
    toast("请先登录");
    return;
  }
  const quantity = Number($(`qty-${productId}`).value || 1);
  await api("/api/orders", {
    method: "POST",
    body: JSON.stringify({ userId: state.user.id, productId, quantity })
  });
  await refresh();
  toast("下单成功");
}

async function cancelOrder(id) {
  await api(`/api/orders/${id}/cancel`, { method: "POST", body: "{}" });
  await refresh();
  toast("订单已取消，库存已恢复");
}

async function checkHealth() {
  try {
    await api("/api/health");
    $("health").textContent = "服务在线";
    $("health").classList.add("ok");
  } catch (error) {
    $("health").textContent = "服务离线";
    $("health").classList.remove("ok");
  }
}

$("loginBtn").addEventListener("click", () => login().catch((err) => toast(err.message)));
$("registerBtn").addEventListener("click", () => register().catch((err) => toast(err.message)));
$("refreshBtn").addEventListener("click", () => refresh().catch((err) => toast(err.message)));

renderUser();
checkHealth();
refresh().catch((err) => toast(err.message));
