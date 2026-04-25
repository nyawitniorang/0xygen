'use strict';
'require view';
'require rpc';

function engRpc(method, args) {
	var a = args || {};
	var paramNames = Object.keys(a);
	var paramValues = paramNames.map(function(k) { return a[k]; });
	var fn = rpc.declare({
		object: 'engsel',
		method: method,
		params: paramNames
	});
	return fn.apply(null, paramValues).catch(function(e) {
		return { error: String(e) };
	});
}

var E = document.createElement.bind(document);

function el(tag, attrs, children) {
	var node = E(tag);
	if (attrs) {
		Object.keys(attrs).forEach(function(k) {
			if (k === 'class') node.className = attrs[k];
			else if (k === 'style' && typeof attrs[k] === 'object') {
				Object.keys(attrs[k]).forEach(function(s) { node.style[s] = attrs[k][s]; });
			} else if (k.substring(0, 2) === 'on') node.addEventListener(k.substring(2), attrs[k]);
			else if (k === 'html') node.innerHTML = attrs[k];
			else node.setAttribute(k, attrs[k]);
		});
	}
	if (children) {
		(Array.isArray(children) ? children : [children]).forEach(function(c) {
			if (c == null) return;
			node.appendChild(typeof c === 'string' ? document.createTextNode(c) : c);
		});
	}
	return node;
}

function fmtRp(n) {
	if (n == null || isNaN(n)) return 'Rp 0';
	return 'Rp ' + Number(n).toLocaleString('id-ID');
}

function fmtBytes(b) {
	if (!b || b <= 0) return '0 B';
	if (b < 1024) return b + ' B';
	if (b < 1048576) return (b / 1024).toFixed(1) + ' KB';
	if (b < 1073741824) return (b / 1048576).toFixed(1) + ' MB';
	return (b / 1073741824).toFixed(2) + ' GB';
}

function showAlert(container, msg, type) {
	var a = el('div', { class: 'eng-alert eng-alert-' + (type || 'info') }, [msg]);
	if (container.firstChild) container.insertBefore(a, container.firstChild);
	else container.appendChild(a);
	setTimeout(function() { a.remove(); }, 5000);
}

function showLoading(container) {
	var ld = el('div', { class: 'eng-loading' }, [
		el('div', { class: 'eng-spinner' }),
		'Memuat...'
	]);
	container.innerHTML = '';
	container.appendChild(ld);
	return ld;
}

var state = {
	page: 'beranda',
	accounts: [],
	activeIdx: 0,
	loggedIn: false,
	number: '',
	stype: '',
	balance: 0,
	expiredAt: '--',
	notifCount: 0
};

var mainContent;
var navItems = {};

/* ── Bottom Tab Navigation (MyXL style) ─────────── */
var NAV = [
	{ id: 'beranda',  icon: '\uD83C\uDFE0', label: 'Beranda' },
	{ id: 'store',    icon: '\uD83D\uDED2', label: 'XL Store' },
	{ id: 'buy',      icon: '\uD83D\uDCE6', label: 'Beli' },
	{ id: 'notif',    icon: '\uD83D\uDD14', label: 'Notif' },
	{ id: 'profil',   icon: '\uD83D\uDC64', label: 'Profil' }
];

function navigate(page) {
	state.page = page;
	Object.keys(navItems).forEach(function(k) {
		navItems[k].classList.toggle('active', k === page);
	});
	renderPage();
}

function renderPage() {
	if (!mainContent) return;
	mainContent.innerHTML = '';
	var renderFn = pages[state.page];
	if (renderFn) renderFn(mainContent);
	else mainContent.appendChild(el('div', { class: 'eng-px eng-mt-2' }, ['Halaman tidak ditemukan']));
	window.scrollTo(0, 0);
}

/* ── Pages ───────────────────────────────────────── */
var pages = {};

/* ═══════════════════════════════════════════════════
   BERANDA (Dashboard - MyXL Home Style)
   ═══════════════════════════════════════════════════ */
pages.beranda = function(ct) {
	/* Profile header */
	var profileHeader = el('div', { class: 'eng-profile-header' }, [
		el('div', { class: 'eng-profile-left' }, [
			el('div', { class: 'eng-avatar' }, ['\uD83D\uDC64']),
			el('div', {}, [
				el('div', { class: 'eng-profile-name' }, [state.loggedIn ? 'Engsel User' : 'Belum Login']),
				el('div', { class: 'eng-profile-number' }, [state.loggedIn ? String(state.number) : 'Login untuk memulai']),
				state.loggedIn ? el('div', { class: 'eng-profile-badge' }, [state.stype || 'PREPAID']) : null
			])
		]),
		el('div', { class: 'eng-notif-btn', onclick: function() { navigate('notif'); } }, [
			'\uD83D\uDD14',
			state.notifCount > 0 ? el('span', { class: 'eng-notif-count' }, [String(state.notifCount)]) : null
		])
	]);
	ct.appendChild(profileHeader);

	if (!state.loggedIn) {
		ct.appendChild(el('div', { class: 'eng-card' }, [
			el('div', { class: 'eng-card-title eng-mb-2' }, ['Selamat Datang di Engsel']),
			el('div', { class: 'eng-text-muted eng-mb-2' }, ['Login untuk mengakses semua fitur MyXL.']),
			el('button', { class: 'eng-btn eng-btn-primary eng-btn-block', onclick: function() { navigate('profil'); } }, ['Login Sekarang'])
		]));
		return;
	}

	/* Banner carousel */
	var bannerWrap = el('div', { class: 'eng-banner-wrap' });
	var bannerScroll = el('div', { class: 'eng-banner-scroll' });
	var banners = [
		{ tag: 'ENGSEL', title: 'MyXL Client', desc: 'Kelola paket, beli kuota, dan atur akun XL langsung dari router OpenWrt.' },
		{ tag: 'PAKET', title: 'Beli Paket Cepat', desc: 'Pilih paket HOT atau cari berdasarkan option code & family code.' },
		{ tag: 'FITUR', title: 'Fitur Lengkap', desc: 'Circle, Transfer Pulsa, Family Plan, Auto Buy, dan lainnya.' }
	];
	banners.forEach(function(b) {
		bannerScroll.appendChild(el('div', { class: 'eng-banner-item' }, [
			el('div', { class: 'eng-banner-tag' }, [b.tag]),
			el('div', { class: 'eng-banner-title' }, [b.title]),
			el('div', { class: 'eng-banner-desc' }, [b.desc])
		]));
	});
	bannerWrap.appendChild(bannerScroll);
	ct.appendChild(bannerWrap);

	/* Quick Menu */
	var quickMenus = [
		{ icon: '\uD83D\uDCB0', label: 'Tagihan', page: 'history' },
		{ icon: '\u2795', label: 'Plan &\nBooster', page: 'packages' },
		{ icon: '\uD83D\uDCB3', label: 'Transfer\nPulsa', page: 'features_transfer' },
		{ icon: '\u2B50', label: 'Promo &\nBookmark', page: 'bookmarks' }
	];
	var quickSection = el('div', { class: 'eng-quick-section' }, [
		el('div', { class: 'eng-section-header' }, [
			el('div', { class: 'eng-section-title' }, ['Menu Cepat']),
			el('div', { class: 'eng-section-link', onclick: function() { navigate('profil'); } }, ['Semua Menu'])
		])
	]);
	var quickGrid = el('div', { class: 'eng-quick-grid' });
	quickMenus.forEach(function(m) {
		quickGrid.appendChild(el('div', { class: 'eng-quick-item', onclick: function() { navigate(m.page); } }, [
			el('div', { class: 'eng-quick-icon' }, [m.icon]),
			el('div', { class: 'eng-quick-label' }, [m.label])
		]));
	});
	quickSection.appendChild(quickGrid);
	ct.appendChild(quickSection);

	/* Quota Summary Card */
	var quotaCard = el('div', { class: 'eng-quota-card' });
	quotaCard.appendChild(el('div', { class: 'eng-quota-card-title' }, ['Lihat Paket Saya']));
	var quotaSummary = el('div', { class: 'eng-quota-summary', id: 'dash-quota-summary' }, [
		el('div', {}, [
			el('div', { class: 'eng-quota-icon' }, ['\uD83C\uDF10']),
			el('div', { class: 'eng-quota-val', id: 'dash-data' }, ['...']),
			el('div', { class: 'eng-quota-sub' }, ['Internet'])
		]),
		el('div', {}, [
			el('div', { class: 'eng-quota-icon' }, ['\uD83D\uDCDE']),
			el('div', { class: 'eng-quota-val', id: 'dash-voice' }, ['-']),
			el('div', { class: 'eng-quota-sub' }, ['Nelpon'])
		]),
		el('div', {}, [
			el('div', { class: 'eng-quota-icon' }, ['\u2709\uFE0F']),
			el('div', { class: 'eng-quota-val', id: 'dash-sms' }, ['-']),
			el('div', { class: 'eng-quota-sub' }, ['SMS'])
		])
	]);
	quotaCard.appendChild(quotaSummary);
	quotaCard.appendChild(el('div', { class: 'eng-quota-link', onclick: function() { navigate('packages'); } }, [
		'Lihat Plan & Booster Saya',
		'\u203A'
	]));
	ct.appendChild(quotaCard);

	/* Balance Row */
	var balanceRow = el('div', { class: 'eng-balance-row' }, [
		el('div', { class: 'eng-balance-card accent-pink' }, [
			el('div', { class: 'eng-balance-label' }, ['Sisa Batas Pemakaian']),
			el('div', { class: 'eng-balance-value', id: 'dash-balance' }, [fmtRp(state.balance)]),
			el('div', { class: 'eng-balance-sub' }, ['Batas Pemakaian'])
		]),
		el('div', { class: 'eng-balance-card accent-green' }, [
			el('div', { class: 'eng-balance-label' }, ['Aktif Sampai']),
			el('div', { class: 'eng-balance-value', id: 'dash-exp' }, [state.expiredAt]),
			el('div', { class: 'eng-balance-sub' }, ['Masa Aktif'])
		])
	]);
	ct.appendChild(balanceRow);

	/* Active packages preview */
	var pkgSection = el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-header' }, [
			el('h3', { class: 'eng-card-title' }, ['Paket Aktif']),
			el('button', { class: 'eng-btn eng-btn-sm', onclick: function() { navigate('packages'); } }, ['Lihat Semua'])
		])
	]);
	var pkgBody = el('div', { id: 'dash-packages' });
	pkgSection.appendChild(pkgBody);
	ct.appendChild(pkgSection);

	loadDashboard(pkgBody);
};

function loadDashboard(pkgContainer) {
	showLoading(pkgContainer);
	engRpc('get_quota').then(function(r) {
		pkgContainer.innerHTML = '';
		var quotas = (r && r.data && r.data.quotas) || (r && r.data) || [];
		if (!Array.isArray(quotas)) quotas = [];

		/* Update summary */
		var totalData = 0, totalVoice = 0, totalSms = 0;
		quotas.forEach(function(q) {
			var remain = parseFloat(q.remaining || q.quota_remaining || 0);
			var btype = (q.benefit_type || q.type || 'DATA').toUpperCase();
			if (btype === 'DATA' || btype === 'INTERNET') totalData += remain;
			else if (btype === 'VOICE') totalVoice += remain;
			else if (btype === 'SMS') totalSms += remain;
		});
		var dashData = document.getElementById('dash-data');
		var dashVoice = document.getElementById('dash-voice');
		var dashSms = document.getElementById('dash-sms');
		if (dashData) dashData.textContent = totalData > 0 ? fmtBytes(totalData) : '-';
		if (dashVoice) dashVoice.textContent = totalVoice > 0 ? Math.round(totalVoice / 60) + ' min' : '-';
		if (dashSms) dashSms.textContent = totalSms > 0 ? String(Math.round(totalSms)) : '-';

		if (quotas.length === 0) {
			pkgContainer.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada paket aktif']));
			return;
		}
		quotas.slice(0, 5).forEach(function(q) {
			var name = q.name || q.quota_name || q.product_name || '-';
			var total = q.total || q.quota_total || '';
			var remain = q.remaining || q.quota_remaining || '';
			var exp = q.expired_at || q.expiry_date || '-';
			if (typeof exp === 'string' && exp.length > 10) exp = exp.substring(0, 10);
			var pct = 0;
			if (total && remain) {
				var t = parseFloat(total), rm = parseFloat(remain);
				if (t > 0) pct = Math.round((rm / t) * 100);
			}
			pkgContainer.appendChild(el('div', { class: 'eng-plan-card eng-card-np', style: { 'margin-left': '0', 'margin-right': '0' } }, [
				el('div', { class: 'eng-plan-header' }, [
					el('div', { class: 'eng-plan-icon' }, ['\uD83C\uDF10']),
					el('div', { class: 'eng-plan-name' }, [name])
				]),
				el('div', { class: 'eng-plan-row' }, [
					el('div', { class: 'eng-plan-row-left' }, ['\uD83C\uDF10 Kuota']),
					el('div', { class: 'eng-plan-row-value' }, [remain ? fmtBytes(parseFloat(remain)) : '-'])
				]),
				el('div', { class: 'eng-quota-bar' }, [
					el('div', { class: 'eng-quota-fill', style: { width: pct + '%' } })
				]),
				total ? el('div', { class: 'eng-plan-total' }, [fmtBytes(parseFloat(total))]) : null,
				el('div', { class: 'eng-plan-row' }, [
					el('div', { class: 'eng-plan-row-left' }, ['\uD83D\uDCC5 Expired']),
					el('div', { class: 'eng-plan-row-value' }, [exp])
				])
			]));
		});
	});
}

/* ═══════════════════════════════════════════════════
   PACKAGES (Plan & Booster - MyXL Style)
   ═══════════════════════════════════════════════════ */
pages.packages = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Plan & Booster Saya'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	function renderTab(tab) {
		tabs.innerHTML = '';
		['Domestik', 'Roaming'].forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t === tab ? ' active' : ''), onclick: function() { renderTab(t); } }, [t]));
		});
		body.innerHTML = '';
		if (tab === 'Domestik') loadPackages(body);
		else {
			body.appendChild(el('div', { class: 'eng-card' }, [
				el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada paket roaming aktif'])
			]));
		}
	}
	ct.appendChild(tabs);
	ct.appendChild(body);
	renderTab('Domestik');
};

function loadPackages(ct) {
	var loadEl = el('div');
	ct.appendChild(loadEl);
	showLoading(loadEl);

	engRpc('get_quota').then(function(r) {
		loadEl.innerHTML = '';
		var quotas = [];
		if (r && r.data) {
			quotas = r.data.quotas || r.data.packages || r.data;
			if (!Array.isArray(quotas)) quotas = [quotas];
		}
		if (quotas.length === 0) {
			loadEl.appendChild(el('div', { class: 'eng-card' }, [
				el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada paket aktif'])
			]));
			return;
		}

		loadEl.appendChild(el('div', { class: 'eng-px eng-mb-2' }, [
			el('div', { class: 'eng-section-title' }, ['Paket Utama'])
		]));

		quotas.forEach(function(q) {
			var name = q.name || q.quota_name || q.product_name || '-';
			var total = q.total || q.quota_total || '';
			var remain = q.remaining || q.quota_remaining || '';
			var exp = q.expired_at || q.expiry_date || '-';
			if (typeof exp === 'string' && exp.length > 10) exp = exp.substring(0, 10);
			var qcode = q.quota_code || q.code || '';
			var stype = q.product_subscription_type || q.subscription_type || '-';
			var domain = q.product_domain || '-';
			var pct = 0;
			if (total && remain) {
				var t = parseFloat(total), rm = parseFloat(remain);
				if (t > 0) pct = Math.round((rm / t) * 100);
			}

			var planCard = el('div', { class: 'eng-plan-card' }, [
				el('div', { class: 'eng-plan-header' }, [
					el('div', { class: 'eng-plan-icon' }, ['\uD83C\uDF10']),
					el('div', {}, [
						el('div', { class: 'eng-plan-name' }, [name]),
						el('div', { class: 'eng-text-sm eng-text-muted' }, [qcode])
					])
				]),
				el('div', { class: 'eng-plan-row' }, [
					el('div', { class: 'eng-plan-row-left' }, ['\uD83C\uDF10 Kuota']),
					el('div', { class: 'eng-plan-row-value' }, [remain ? fmtBytes(parseFloat(remain)) : '-'])
				]),
				el('div', { class: 'eng-quota-bar' }, [
					el('div', { class: 'eng-quota-fill', style: { width: pct + '%' } })
				]),
				total ? el('div', { class: 'eng-plan-total' }, [fmtBytes(parseFloat(total))]) : null,
				el('div', { class: 'eng-plan-row' }, [
					el('div', { class: 'eng-plan-row-left' }, ['\uD83D\uDCC5 Reset Kuota']),
					el('div', { class: 'eng-plan-row-value' }, [exp])
				]),
				el('div', { class: 'eng-plan-row' }, [
					el('div', { class: 'eng-plan-row-left' }, ['\uD83D\uDCCB Tipe']),
					el('div', {}, [el('span', { class: 'eng-badge eng-badge-info' }, [stype])])
				]),
				qcode ? el('div', { class: 'eng-mt-2' }, [
					el('button', { class: 'eng-btn eng-btn-danger eng-btn-block', 'data-qc': qcode, 'data-st': stype, 'data-dm': domain, onclick: function() {
						if (!confirm('Unsub paket ini?')) return;
						engRpc('unsubscribe', {
							quota_code: this.getAttribute('data-qc'),
							product_subscription_type: this.getAttribute('data-st'),
							product_domain: this.getAttribute('data-dm')
						}).then(function(ur) {
							showAlert(loadEl, JSON.stringify(ur), ur.error ? 'danger' : 'success');
							navigate('packages');
						});
					} }, ['Ubah Plan'])
				]) : null
			]);
			loadEl.appendChild(planCard);
		});

		loadEl.appendChild(el('div', { class: 'eng-px eng-mt-2' }, [
			el('button', { class: 'eng-btn eng-btn-primary eng-btn-block eng-btn-lg', onclick: function() { navigate('store'); } }, ['Tambah Booster'])
		]));
	});
}

/* ═══════════════════════════════════════════════════
   XL STORE (Store tabs - MyXL Style)
   ═══════════════════════════════════════════════════ */
pages.store = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['XL Store'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	var storeTabs = [
		{ id: 'family', label: 'Tarif Dasar', icon: '\uD83D\uDCCB' },
		{ id: 'packages', label: 'Tambah Kuota', icon: '\u2795' },
		{ id: 'segments', label: 'Segments', icon: '\uD83C\uDFAF' },
		{ id: 'redeemables', label: 'Redeem', icon: '\uD83C\uDF81' }
	];

	function renderStoreTab(tabId) {
		tabs.innerHTML = '';
		storeTabs.forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t.id === tabId ? ' active' : ''), onclick: function() { renderStoreTab(t.id); } }, [t.icon + ' ' + t.label]));
		});
		body.innerHTML = '';
		showLoading(body);
		var rpcMethod = tabId === 'family' ? 'store_family_list' :
		               tabId === 'packages' ? 'store_packages' :
		               tabId === 'segments' ? 'store_segments' : 'store_redeemables';
		engRpc(rpcMethod).then(function(r) {
			body.innerHTML = '';
			if (r && r.error) { showAlert(body, 'Error: ' + r.error, 'danger'); return; }
			var data = (r && r.data) || r || {};
			var items = data.families || data.packages || data.segments || data.redeemables || [];
			if (!Array.isArray(items)) items = [items];
			if (items.length === 0) {
				body.appendChild(el('div', { class: 'eng-card' }, [
					el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada data'])
				]));
				return;
			}

			var storeContent = el('div', { class: 'eng-px' });
			items.forEach(function(item, idx) {
				var name = item.name || item.family_name || item.segment_name || '-';
				var code = item.code || item.family_code || item.option_code || '';
				var price = item.price || item.base_price || null;
				var desc = item.description || '';
				var isFeatured = idx === 0;

				var card = el('div', { class: 'eng-family-card' + (isFeatured ? ' featured' : ''), onclick: function() {
					if (code && price != null) {
						showPurchaseModal({ option_code: code, name: name, price: price, token_confirmation: item.token_confirmation || '', payment_for: item.payment_for || 'BUY_PACKAGE' });
					} else if (item.family_code) {
						navigate('buy');
					}
				} }, [
					el('div', { class: 'eng-family-info' }, [
						isFeatured ? el('div', { class: 'eng-family-badge' }, ['NEW']) : null,
						el('div', { class: 'eng-family-name' }, [name]),
						el('div', { class: 'eng-family-desc' }, [desc ? desc.substring(0, 80) : (price != null ? fmtRp(price) : 'Kode: ' + code)])
					]),
					el('div', { class: 'eng-family-icon' }, [
						isFeatured ? '\uD83D\uDE80' : (tabId === 'family' ? '\uD83D\uDCCB' : tabId === 'packages' ? '\uD83D\uDCE6' : '\uD83C\uDF81')
					])
				]);
				storeContent.appendChild(card);
			});
			body.appendChild(storeContent);
		});
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderStoreTab('family');
};

/* ═══════════════════════════════════════════════════
   BUY (Purchase Packages)
   ═══════════════════════════════════════════════════ */
pages.buy = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Beli Paket'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	var buyTabs = ['HOT', 'Option Code', 'Family Code', 'Loop Family'];
	var activeTab = 'HOT';

	function renderBuyTab(tab) {
		activeTab = tab;
		tabs.innerHTML = '';
		buyTabs.forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t === tab ? ' active' : ''), onclick: function() { renderBuyTab(t); } }, [t]));
		});
		body.innerHTML = '';
		if (tab === 'HOT') renderBuyHot(body);
		else if (tab === 'Option Code') renderBuyOption(body);
		else if (tab === 'Family Code') renderBuyFamily(body);
		else if (tab === 'Loop Family') renderBuyLoop(body);
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderBuyTab('HOT');
};

function renderBuyHot(ct) {
	var card = el('div', { class: 'eng-card' });
	var body = el('div');
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['\uD83D\uDD25 Paket HOT']));
	card.appendChild(body);
	ct.appendChild(card);
	showLoading(body);

	engRpc('get_hot').then(function(r) {
		body.innerHTML = '';
		var pkgs = (r && r.packages) || [];
		if (pkgs.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-muted' }, ['Belum ada paket HOT. Tambahkan di /etc/engsel/hot_data/hot.json']));
			return;
		}
		var grid = el('div', { class: 'eng-card-grid' });
		pkgs.forEach(function(p) {
			grid.appendChild(el('div', { class: 'eng-pkg-card', onclick: function() { showPurchaseModal(p); } }, [
				el('div', { class: 'eng-pkg-name' }, [p.name || p.option_code || '-']),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(p.price || 0)]),
				el('div', { class: 'eng-pkg-meta' }, [
					el('span', {}, ['Code: ' + (p.option_code || '-')]),
					p.family_code ? el('span', {}, ['Family: ' + p.family_code]) : null
				])
			]));
		});
		body.appendChild(grid);
	});
}

function renderBuyOption(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Beli Berdasarkan Option Code']));

	var optInput = el('input', { class: 'eng-input', placeholder: 'Masukkan option code', type: 'text' });
	var resultDiv = el('div', { class: 'eng-mt-2' });
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-mt-1' }, ['Cari']);

	btn.addEventListener('click', function() {
		var oc = optInput.value.trim();
		if (!oc) return;
		showLoading(resultDiv);
		engRpc('get_package_detail', { option_code: oc }).then(function(r) {
			resultDiv.innerHTML = '';
			if (r && r.error) {
				showAlert(resultDiv, 'Error: ' + r.error, 'danger');
				return;
			}
			var pkg = (r && r.data && r.data.package_detail) || (r && r.data) || r || {};
			var name = pkg.name || pkg.package_name || oc;
			var price = pkg.price || pkg.base_price || 0;
			var desc = pkg.description || pkg.desc || '';
			var conf = pkg.token_confirmation || pkg.confirmation_token || '';
			var payFor = pkg.payment_for || 'BUY_PACKAGE';

			resultDiv.appendChild(el('div', { class: 'eng-pkg-card' }, [
				el('div', { class: 'eng-pkg-name' }, [name]),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(price)]),
				desc ? el('div', { class: 'eng-text-sm eng-text-muted eng-mt-1' }, [desc]) : null,
				el('div', { class: 'eng-mt-2' }, [
					el('button', { class: 'eng-btn eng-btn-primary eng-btn-block', onclick: function() {
						showPurchaseModal({ option_code: oc, name: name, price: price, token_confirmation: conf, payment_for: payFor });
					} }, ['Beli Paket Ini'])
				])
			]));
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Option Code']), optInput]));
	card.appendChild(btn);
	card.appendChild(resultDiv);
	ct.appendChild(card);
}

function renderBuyFamily(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Beli Berdasarkan Family Code']));

	var famInput = el('input', { class: 'eng-input', placeholder: 'Masukkan family code', type: 'text' });
	var resultDiv = el('div', { class: 'eng-mt-2' });
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-mt-1' }, ['Cari (Auto Bruteforce)']);

	btn.addEventListener('click', function() {
		var fc = famInput.value.trim();
		if (!fc) return;
		showLoading(resultDiv);
		engRpc('family_bruteforce', { family_code: fc }).then(function(r) {
			resultDiv.innerHTML = '';
			if (r && r.error) {
				showAlert(resultDiv, 'Family tidak ditemukan: ' + r.error, 'danger');
				return;
			}
			var variants = (r && r.data && r.data.package_variants) || [];
			if (variants.length === 0) {
				showAlert(resultDiv, 'Tidak ada paket dalam family ini', 'warning');
				return;
			}
			var grid = el('div', { class: 'eng-card-grid' });
			variants.forEach(function(v) {
				var name = v.name || v.package_name || '-';
				var price = v.price || v.base_price || 0;
				var oc = v.option_code || v.package_option_code || '';
				var conf = v.token_confirmation || '';
				var payFor = v.payment_for || 'BUY_PACKAGE';
				grid.appendChild(el('div', { class: 'eng-pkg-card', onclick: function() {
					showPurchaseModal({ option_code: oc, name: name, price: price, token_confirmation: conf, payment_for: payFor, family_code: fc });
				} }, [
					el('div', { class: 'eng-pkg-name' }, [name]),
					el('div', { class: 'eng-pkg-price' }, [fmtRp(price)]),
					el('div', { class: 'eng-pkg-meta' }, [el('span', {}, ['Code: ' + oc])])
				]));
			});
			resultDiv.appendChild(grid);
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Family Code']), famInput]));
	card.appendChild(btn);
	card.appendChild(resultDiv);
	ct.appendChild(card);
}

function renderBuyLoop(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Beli Semua Paket di Family (Loop)']));

	var famInput = el('input', { class: 'eng-input', placeholder: 'Family code', type: 'text' });
	var delayInput = el('input', { class: 'eng-input', placeholder: 'Delay antar pembelian (detik)', type: 'number', value: '2' });
	var resultDiv = el('div', { class: 'eng-mt-2' });
	var btn = el('button', { class: 'eng-btn eng-btn-gold eng-btn-lg eng-btn-block eng-mt-1' }, ['Mulai Loop Pembelian']);
	var running = false;

	btn.addEventListener('click', function() {
		var fc = famInput.value.trim();
		if (!fc) return;
		if (running) return;
		running = true;
		btn.disabled = true;
		btn.textContent = 'Mencari paket...';
		resultDiv.innerHTML = '';

		engRpc('family_bruteforce', { family_code: fc }).then(function(r) {
			if (r && r.error) {
				showAlert(resultDiv, 'Gagal: ' + r.error, 'danger');
				running = false; btn.disabled = false; btn.textContent = 'Mulai Loop Pembelian';
				return;
			}
			var variants = (r && r.data && r.data.package_variants) || [];
			if (variants.length === 0) {
				showAlert(resultDiv, 'Tidak ada paket', 'warning');
				running = false; btn.disabled = false; btn.textContent = 'Mulai Loop Pembelian';
				return;
			}
			btn.textContent = 'Membeli ' + variants.length + ' paket...';
			var delay = parseInt(delayInput.value) || 2;
			var logEl = el('div', { class: 'eng-card eng-card-np', style: { 'max-height': '400px', 'overflow-y': 'auto' } });
			resultDiv.appendChild(logEl);

			function buyNext(idx) {
				if (idx >= variants.length) {
					logEl.appendChild(el('div', { class: 'eng-alert eng-alert-success' }, ['Selesai! ' + variants.length + ' paket diproses.']));
					running = false; btn.disabled = false; btn.textContent = 'Mulai Loop Pembelian';
					return;
				}
				var v = variants[idx];
				var name = v.name || v.package_name || v.option_code;
				logEl.appendChild(el('div', { class: 'eng-text-sm eng-mb-1' }, ['[' + (idx + 1) + '/' + variants.length + '] Membeli: ' + name + '...']));
				engRpc('purchase_balance', {
					option_code: v.option_code || v.package_option_code || '',
					price: v.price || v.base_price || 0,
					name: name,
					token_confirmation: v.token_confirmation || '',
					payment_for: v.payment_for || 'BUY_PACKAGE',
					overwrite_amount: v.price || v.base_price || 0
				}).then(function(pr) {
					var st = (pr && pr.status) || (pr && pr.error) || 'unknown';
					var cls = st === 'SUCCESS' ? 'eng-alert-success' : 'eng-alert-warning';
					logEl.appendChild(el('div', { class: 'eng-alert ' + cls + ' eng-text-sm' }, ['  => ' + name + ': ' + st]));
					logEl.scrollTop = logEl.scrollHeight;
					setTimeout(function() { buyNext(idx + 1); }, delay * 1000);
				});
			}
			buyNext(0);
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Family Code']), famInput]));
	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Delay (detik)']), delayInput]));
	card.appendChild(btn);
	card.appendChild(resultDiv);
	ct.appendChild(card);
}

/* Purchase modal */
function showPurchaseModal(pkg) {
	var overlay = el('div', { class: 'eng-modal-overlay' });
	var modal = el('div', { class: 'eng-modal' });

	var methodSelect = el('select', { class: 'eng-select' }, [
		el('option', { value: 'balance' }, ['1. Pulsa Biasa']),
		el('option', { value: 'decoy' }, ['2. Pulsa + Decoy (Bypass)']),
		el('option', { value: 'ntimes' }, ['3. Pulsa N kali']),
		el('option', { value: 'ewallet' }, ['4. E-Wallet']),
		el('option', { value: 'qris' }, ['5. QRIS'])
	]);

	var extraFields = el('div', { class: 'eng-mt-2' });
	var nTimesInput = el('input', { class: 'eng-input', type: 'number', placeholder: 'Jumlah pembelian', value: '1', style: { display: 'none' } });
	var walletSelect = el('select', { class: 'eng-select', style: { display: 'none' } }, [
		el('option', { value: 'DANA' }, ['DANA']),
		el('option', { value: 'SHOPEE_PAY' }, ['ShopeePay']),
		el('option', { value: 'GO_PAY' }, ['GoPay']),
		el('option', { value: 'OVO' }, ['OVO'])
	]);
	var walletNumInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Nomor E-Wallet', style: { display: 'none' } });

	methodSelect.addEventListener('change', function() {
		nTimesInput.style.display = 'none';
		walletSelect.style.display = 'none';
		walletNumInput.style.display = 'none';
		if (this.value === 'ntimes') nTimesInput.style.display = '';
		if (this.value === 'ewallet') { walletSelect.style.display = ''; walletNumInput.style.display = ''; }
	});

	extraFields.appendChild(nTimesInput);
	extraFields.appendChild(walletSelect);
	extraFields.appendChild(walletNumInput);

	var resultEl = el('div', { class: 'eng-mt-2' });

	modal.appendChild(el('h3', {}, ['Beli Paket']));
	modal.appendChild(el('div', { class: 'eng-mb-2' }, [
		el('div', { class: 'eng-pkg-name' }, [pkg.name || pkg.option_code || '-']),
		el('div', { class: 'eng-pkg-price' }, [fmtRp(pkg.price)]),
		el('div', { class: 'eng-text-sm eng-text-muted' }, ['Code: ' + (pkg.option_code || '-')])
	]));
	modal.appendChild(el('div', { class: 'eng-form-group' }, [
		el('label', {}, ['Metode Pembayaran']),
		methodSelect
	]));
	modal.appendChild(extraFields);

	var btnBuy = el('button', { class: 'eng-btn eng-btn-primary eng-btn-lg eng-btn-block' }, ['Beli Sekarang']);
	var btnCancel = el('button', { class: 'eng-btn eng-btn-block eng-mt-1' }, ['Batal']);

	btnBuy.addEventListener('click', function() {
		var method = methodSelect.value;
		btnBuy.disabled = true;
		btnBuy.textContent = 'Memproses...';

		function doPurchase() {
			var rpcMethod, rpcArgs;
			if (method === 'balance' || method === 'decoy' || method === 'ntimes') {
				rpcMethod = 'purchase_balance';
				rpcArgs = {
					option_code: pkg.option_code || '',
					price: pkg.price || 0,
					name: pkg.name || '',
					token_confirmation: pkg.token_confirmation || '',
					payment_for: pkg.payment_for || 'BUY_PACKAGE',
					overwrite_amount: pkg.price || 0
				};
			} else if (method === 'ewallet') {
				rpcMethod = 'purchase_ewallet';
				rpcArgs = {
					option_code: pkg.option_code || '',
					price: pkg.price || 0,
					name: pkg.name || '',
					token_confirmation: pkg.token_confirmation || '',
					payment_for: pkg.payment_for || 'BUY_PACKAGE',
					wallet_number: walletNumInput.value.trim(),
					payment_method: walletSelect.value
				};
			} else {
				rpcMethod = 'purchase_qris';
				rpcArgs = {
					option_code: pkg.option_code || '',
					price: pkg.price || 0,
					name: pkg.name || '',
					token_confirmation: pkg.token_confirmation || '',
					payment_for: pkg.payment_for || 'BUY_PACKAGE'
				};
			}
			return engRpc(rpcMethod, rpcArgs);
		}

		if (method === 'ntimes') {
			var count = parseInt(nTimesInput.value) || 1;
			var completed = 0;
			function buyOne() {
				if (completed >= count) {
					resultEl.appendChild(el('div', { class: 'eng-alert eng-alert-success' }, ['Selesai! ' + count + ' pembelian diproses.']));
					btnBuy.disabled = false; btnBuy.textContent = 'Beli Sekarang';
					return;
				}
				doPurchase().then(function(r) {
					completed++;
					var st = (r && r.status) || 'unknown';
					resultEl.appendChild(el('div', { class: 'eng-text-sm' }, ['[' + completed + '/' + count + '] ' + st]));
					setTimeout(buyOne, 1500);
				});
			}
			buyOne();
		} else {
			doPurchase().then(function(r) {
				btnBuy.disabled = false; btnBuy.textContent = 'Beli Sekarang';
				if (r && r.data && r.data.qr_url) {
					resultEl.innerHTML = '';
					resultEl.appendChild(el('div', { class: 'eng-alert eng-alert-info' }, ['Scan QRIS di bawah:']));
					resultEl.appendChild(el('img', { src: r.data.qr_url, style: { 'max-width': '280px' } }));
				} else {
					var st = (r && r.status) || JSON.stringify(r);
					var cls = st === 'SUCCESS' ? 'eng-alert-success' : 'eng-alert-warning';
					resultEl.innerHTML = '';
					resultEl.appendChild(el('div', { class: 'eng-alert ' + cls }, [st]));
				}
			});
		}
	});

	btnCancel.addEventListener('click', function() { overlay.remove(); });
	overlay.addEventListener('click', function(e) { if (e.target === overlay) overlay.remove(); });

	modal.appendChild(el('div', { class: 'eng-mt-2' }, [btnBuy, btnCancel]));
	modal.appendChild(resultEl);
	overlay.appendChild(modal);
	document.body.appendChild(overlay);
}

/* ═══════════════════════════════════════════════════
   NOTIF (Notifications)
   ═══════════════════════════════════════════════════ */
pages.notif = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Notifikasi'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var body = el('div', { class: 'eng-px' });
	ct.appendChild(body);
	showLoading(body);

	engRpc('notifications').then(function(r) {
		body.innerHTML = '';
		if (r && r.error) { showAlert(body, 'Error: ' + r.error, 'danger'); return; }
		var notifs = (r && r.data && r.data.notifications) || (r && r.data) || [];
		if (!Array.isArray(notifs)) notifs = [notifs];
		if (notifs.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-muted eng-text-center eng-mt-2' }, ['Tidak ada notifikasi']));
			return;
		}
		notifs.forEach(function(n) {
			var title = n.title || n.name || '-';
			var msg = n.message || n.body || n.description || '';
			var date = n.date || n.created_at || '';
			body.appendChild(el('div', { class: 'eng-family-card', onclick: function() {
				if (n.id || n.notification_id) {
					engRpc('notification_detail', { notification_id: n.id || n.notification_id }).then(function(d) {
						var detail = (d && d.data) || d || {};
						alert(JSON.stringify(detail, null, 2));
					});
				}
			} }, [
				el('div', { class: 'eng-family-info' }, [
					el('div', { class: 'eng-family-name' }, [title]),
					msg ? el('div', { class: 'eng-family-desc' }, [msg]) : null
				]),
				el('div', { style: { 'font-size': '11px', color: 'var(--eng-text2)', 'white-space': 'nowrap' } }, [date])
			]));
		});
	});
};

/* ═══════════════════════════════════════════════════
   PROFIL (Account Management + All Features)
   ═══════════════════════════════════════════════════ */
pages.profil = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Profil & Akun'])
	]));

	/* Login form */
	var loginCard = el('div', { class: 'eng-card' }, [
		el('h3', { class: 'eng-card-title eng-mb-2' }, ['Login / Tambah Akun'])
	]);

	var loginForm = el('div');
	var numInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Nomor HP (08xx / 628xx)' });
	var otpInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Kode OTP', style: { display: 'none' } });
	var loginMsg = el('div', { class: 'eng-mt-1' });
	var btnOtp = el('button', { class: 'eng-btn eng-btn-primary eng-btn-block eng-mt-1' }, ['Kirim OTP']);
	var btnSubmit = el('button', { class: 'eng-btn eng-btn-success eng-btn-block eng-mt-1', style: { display: 'none' } }, ['Verifikasi']);
	var loginNumber = '';

	btnOtp.addEventListener('click', function() {
		loginNumber = numInput.value.trim();
		if (!loginNumber) return;
		btnOtp.disabled = true;
		btnOtp.textContent = 'Mengirim...';
		engRpc('request_otp', { number: loginNumber }).then(function(r) {
			btnOtp.disabled = false;
			btnOtp.textContent = 'Kirim OTP';
			if (r && r.error) {
				loginMsg.textContent = 'Gagal: ' + r.error + (r.detail ? ' - ' + r.detail : '');
				loginMsg.className = 'eng-mt-1 eng-alert eng-alert-danger';
				return;
			}
			loginMsg.textContent = 'OTP dikirim ke ' + loginNumber;
			loginMsg.className = 'eng-mt-1 eng-alert eng-alert-success';
			otpInput.style.display = '';
			btnSubmit.style.display = '';
			btnOtp.style.display = 'none';
			otpInput.focus();
		});
	});

	btnSubmit.addEventListener('click', function() {
		var code = otpInput.value.trim();
		if (!code) return;
		btnSubmit.disabled = true;
		btnSubmit.textContent = 'Memverifikasi...';
		engRpc('submit_otp', { number: loginNumber, code: code }).then(function(r) {
			btnSubmit.disabled = false;
			btnSubmit.textContent = 'Verifikasi';
			if (r && r.error) {
				loginMsg.textContent = 'Gagal: ' + r.error + (r.detail ? ' - ' + r.detail : '');
				loginMsg.className = 'eng-mt-1 eng-alert eng-alert-danger';
				return;
			}
			state.loggedIn = true;
			state.number = r.number || loginNumber;
			state.stype = r.subscription_type || 'PREPAID';
			loginMsg.textContent = 'Login berhasil!';
			loginMsg.className = 'eng-mt-1 eng-alert eng-alert-success';
			numInput.value = '';
			otpInput.value = '';
			otpInput.style.display = 'none';
			btnSubmit.style.display = 'none';
			btnOtp.style.display = '';
			refreshAccounts(accountList);
			engRpc('refresh_auth').then(function(ar) {
				if (ar && ar.balance != null) {
					state.balance = ar.balance;
					state.expiredAt = ar.expired_at || '--';
				}
			});
		});
	});

	loginForm.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Nomor HP']), numInput]));
	loginForm.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Kode OTP']), otpInput]));
	loginForm.appendChild(el('div', { class: 'eng-btn-group', style: { 'flex-direction': 'column' } }, [btnOtp, btnSubmit]));
	loginForm.appendChild(loginMsg);
	loginCard.appendChild(loginForm);
	ct.appendChild(loginCard);

	/* Account list */
	var accountCard = el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-header' }, [
			el('h3', { class: 'eng-card-title' }, ['Daftar Akun']),
			el('button', { class: 'eng-btn eng-btn-sm', onclick: function() { refreshAccounts(accountList); } }, ['Refresh'])
		])
	]);
	var accountList = el('div');
	accountCard.appendChild(accountList);
	ct.appendChild(accountCard);
	refreshAccounts(accountList);

	/* All Menu Grid */
	ct.appendChild(el('div', { class: 'eng-px eng-mt-2 eng-mb-2' }, [
		el('div', { class: 'eng-section-title' }, ['Semua Menu'])
	]));
	var allMenus = [
		{ icon: '\uD83D\uDCCA', label: 'Riwayat', page: 'history' },
		{ icon: '\u2699\uFE0F', label: 'Fitur Lanjutan', page: 'features' },
		{ icon: '\uD83D\uDCDD', label: 'Registrasi', page: 'register' },
		{ icon: '\u2B50', label: 'Bookmark', page: 'bookmarks' },
		{ icon: '\uD83D\uDD27', label: 'Pengaturan', page: 'settings' }
	];
	var menuGrid = el('div', { class: 'eng-quick-grid eng-px', style: { 'grid-template-columns': 'repeat(3, 1fr)' } });
	allMenus.forEach(function(m) {
		menuGrid.appendChild(el('div', { class: 'eng-quick-item', onclick: function() { navigate(m.page); } }, [
			el('div', { class: 'eng-quick-icon' }, [m.icon]),
			el('div', { class: 'eng-quick-label' }, [m.label])
		]));
	});
	ct.appendChild(menuGrid);
};

function refreshAccounts(container) {
	showLoading(container);
	engRpc('list_accounts').then(function(r) {
		container.innerHTML = '';
		var accounts = (r && r.accounts) || [];
		state.accounts = accounts;
		state.activeIdx = (r && r.active) || 0;
		if (accounts.length === 0) {
			container.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Belum ada akun. Login untuk menambahkan.']));
			return;
		}
		accounts.forEach(function(acc, i) {
			var isActive = acc.active || (i === state.activeIdx);
			container.appendChild(el('div', { class: 'eng-family-card' + (isActive ? ' featured' : '') }, [
				el('div', { class: 'eng-family-info' }, [
					el('div', { class: 'eng-family-name' }, [String(acc.number || '-')]),
					el('div', { class: 'eng-family-desc' }, [
						(acc.subscription_type || 'N/A') + ' \u2022 ' + (isActive ? 'AKTIF' : 'IDLE')
					])
				]),
				el('div', { class: 'eng-btn-group', style: { 'flex-direction': 'column' } }, [
					!isActive ? el('button', { class: 'eng-btn eng-btn-sm eng-btn-primary', 'data-idx': String(i), onclick: function() {
						var idx = parseInt(this.getAttribute('data-idx'));
						showLoading(container);
						engRpc('switch_account', { index: idx }).then(function(sr) {
							if (sr && sr.status === 'ok') {
								state.loggedIn = true;
								state.number = sr.number || '';
								state.stype = sr.subscription_type || '';
								state.balance = sr.balance || 0;
								state.expiredAt = sr.expired_at || '--';
							}
							refreshAccounts(container);
						});
					} }, ['Switch']) : null,
					el('button', { class: 'eng-btn eng-btn-sm eng-btn-danger', 'data-idx': String(i), onclick: function() {
						if (!confirm('Hapus akun ' + acc.number + '?')) return;
						var idx = parseInt(this.getAttribute('data-idx'));
						engRpc('delete_account', { index: idx }).then(function() { refreshAccounts(container); });
					} }, ['Hapus'])
				])
			]));
		});
	});
}

/* ═══════════════════════════════════════════════════
   SUB-PAGES (accessed via Profil menu)
   ═══════════════════════════════════════════════════ */

/* Transaction History */
pages.history = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Riwayat Transaksi'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');

	function renderHistoryTab(tab) {
		tabs.innerHTML = '';
		['Riwayat', 'Pending'].forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t === tab ? ' active' : ''), onclick: function() { renderHistoryTab(t); } }, [t]));
		});
		body.innerHTML = '';
		showLoading(body);
		var rpcMethod = tab === 'Riwayat' ? 'transaction_history' : 'pending_payments';
		engRpc(rpcMethod).then(function(r) {
			body.innerHTML = '';
			if (r && r.error) { showAlert(body, 'Error: ' + r.error, 'danger'); return; }
			var txs = (r && r.data && r.data.transactions) || (r && r.data && r.data.payments) || (r && r.data) || [];
			if (!Array.isArray(txs)) txs = [txs];
			if (txs.length === 0) {
				body.appendChild(el('div', { class: 'eng-card' }, [el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada data'])]));
				return;
			}
			var tableWrap = el('div', { class: 'eng-card', style: { 'overflow-x': 'auto' } });
			var table = el('table', { class: 'eng-table' });
			table.appendChild(el('thead', {}, [el('tr', {}, [
				el('th', {}, ['Tanggal']),
				el('th', {}, ['Nama']),
				el('th', {}, ['Harga']),
				el('th', {}, ['Status']),
				el('th', {}, ['Metode'])
			])]));
			var tbody = el('tbody');
			txs.forEach(function(tx) {
				var date = tx.date || tx.created_at || tx.transaction_date || '-';
				if (typeof date === 'string' && date.length > 16) date = date.substring(0, 16);
				var name = tx.name || tx.package_name || '-';
				var price = tx.price || tx.amount || 0;
				var status = tx.status || '-';
				var method = tx.payment_method || tx.method || '-';
				var statusClass = status === 'SUCCESS' ? 'eng-badge-success' : status === 'PENDING' ? 'eng-badge-warning' : 'eng-badge-danger';
				tbody.appendChild(el('tr', {}, [
					el('td', { class: 'eng-text-sm' }, [date]),
					el('td', {}, [name]),
					el('td', {}, [fmtRp(price)]),
					el('td', {}, [el('span', { class: 'eng-badge ' + statusClass }, [status])]),
					el('td', { class: 'eng-text-sm' }, [method])
				]));
			});
			table.appendChild(tbody);
			tableWrap.appendChild(table);
			body.appendChild(tableWrap);
		});
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderHistoryTab('Riwayat');
};

/* Advanced Features */
pages.features = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Fitur Lanjutan'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	var fTabs = ['Circle', 'Family Plan', 'Transfer Pulsa', 'Validate MSISDN'];

	function renderFeatureTab(tab) {
		tabs.innerHTML = '';
		fTabs.forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t === tab ? ' active' : ''), onclick: function() { renderFeatureTab(t); } }, [t]));
		});
		body.innerHTML = '';
		if (tab === 'Circle') renderCircle(body);
		else if (tab === 'Family Plan') renderFamplan(body);
		else if (tab === 'Transfer Pulsa') renderTransfer(body);
		else if (tab === 'Validate MSISDN') renderValidate(body);
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderFeatureTab('Circle');
};

/* Transfer direct page */
pages.features_transfer = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Transfer Pulsa'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	renderTransfer(ct);
};

function renderCircle(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Circle Data']));
	var body = el('div');
	card.appendChild(body);
	ct.appendChild(card);
	showLoading(body);
	engRpc('circle_data').then(function(r) {
		body.innerHTML = '';
		if (r && r.error) { showAlert(body, 'Error: ' + r.error, 'danger'); return; }
		var data = (r && r.data) || r || {};
		body.appendChild(el('pre', { style: { color: 'var(--eng-text2)', 'font-size': '12px', 'white-space': 'pre-wrap', 'word-break': 'break-all' } }, [JSON.stringify(data, null, 2)]));
	});
}

function renderFamplan(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Family Plan / Akrab Organizer']));
	var body = el('div');
	card.appendChild(body);
	ct.appendChild(card);
	showLoading(body);
	engRpc('famplan_info').then(function(r) {
		body.innerHTML = '';
		if (r && r.error) { showAlert(body, 'Error: ' + r.error, 'danger'); return; }
		var data = (r && r.data) || r || {};
		body.appendChild(el('pre', { style: { color: 'var(--eng-text2)', 'font-size': '12px', 'white-space': 'pre-wrap', 'word-break': 'break-all' } }, [JSON.stringify(data, null, 2)]));
	});
}

function renderTransfer(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Transfer Pulsa']));

	var recvInput = el('input', { class: 'eng-input', placeholder: 'Nomor tujuan (08xx)', type: 'text' });
	var amountInput = el('input', { class: 'eng-input', placeholder: 'Jumlah (Rp)', type: 'number' });
	var pinInput = el('input', { class: 'eng-input', placeholder: 'PIN Transfer', type: 'password' });
	var resultDiv = el('div', { class: 'eng-mt-2' });
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-lg eng-btn-block' }, ['Transfer']);

	btn.addEventListener('click', function() {
		var recv = recvInput.value.trim();
		var amount = parseInt(amountInput.value);
		var pin = pinInput.value.trim();
		if (!recv || !amount || !pin) { showAlert(resultDiv, 'Lengkapi semua field', 'warning'); return; }
		btn.disabled = true;
		btn.textContent = 'Memproses...';
		engRpc('transfer_balance', { receiver: recv, amount: amount, pin: pin }).then(function(r) {
			btn.disabled = false;
			btn.textContent = 'Transfer';
			resultDiv.innerHTML = '';
			var st = (r && r.status) || (r && r.error) || JSON.stringify(r);
			var cls = (r && !r.error) ? 'eng-alert-success' : 'eng-alert-danger';
			resultDiv.appendChild(el('div', { class: 'eng-alert ' + cls }, [String(st)]));
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Nomor Tujuan']), recvInput]));
	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Jumlah (Rp)']), amountInput]));
	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['PIN']), pinInput]));
	card.appendChild(btn);
	card.appendChild(resultDiv);
	ct.appendChild(card);
}

function renderValidate(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Validate MSISDN']));
	var msisdnInput = el('input', { class: 'eng-input', placeholder: 'Nomor MSISDN', type: 'text' });
	var resultDiv = el('div', { class: 'eng-mt-2' });
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-block' }, ['Validate']);

	btn.addEventListener('click', function() {
		var m = msisdnInput.value.trim();
		if (!m) return;
		showLoading(resultDiv);
		engRpc('validate_msisdn', { msisdn: m }).then(function(r) {
			resultDiv.innerHTML = '';
			var data = (r && r.data) || r || {};
			resultDiv.appendChild(el('pre', { style: { color: 'var(--eng-text2)', 'font-size': '12px', 'white-space': 'pre-wrap' } }, [JSON.stringify(data, null, 2)]));
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['MSISDN']), msisdnInput]));
	card.appendChild(btn);
	card.appendChild(resultDiv);
	ct.appendChild(card);
}

/* Register */
pages.register = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Registrasi Kartu'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Registrasi via Dukcapil']));

	var msisdnInput = el('input', { class: 'eng-input', placeholder: 'Nomor MSISDN', type: 'text' });
	var nikInput = el('input', { class: 'eng-input', placeholder: 'NIK (16 digit)', type: 'text' });
	var kkInput = el('input', { class: 'eng-input', placeholder: 'No. KK (16 digit)', type: 'text' });
	var resultDiv = el('div', { class: 'eng-mt-2' });
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-lg eng-btn-block' }, ['Registrasi']);

	btn.addEventListener('click', function() {
		var m = msisdnInput.value.trim();
		var nik = nikInput.value.trim();
		var kk = kkInput.value.trim();
		if (!m || !nik || !kk) { showAlert(resultDiv, 'Lengkapi semua field', 'warning'); return; }
		btn.disabled = true;
		btn.textContent = 'Memproses...';
		engRpc('register_card', { msisdn: m, nik: nik, kk: kk }).then(function(r) {
			btn.disabled = false;
			btn.textContent = 'Registrasi';
			resultDiv.innerHTML = '';
			var st = (r && r.status) || JSON.stringify(r);
			var cls = (r && !r.error) ? 'eng-alert-success' : 'eng-alert-danger';
			resultDiv.appendChild(el('div', { class: 'eng-alert ' + cls }, [String(st)]));
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['MSISDN']), msisdnInput]));
	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['NIK']), nikInput]));
	card.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['No. KK']), kkInput]));
	card.appendChild(btn);
	card.appendChild(resultDiv);
	ct.appendChild(card);
};

/* Bookmarks */
pages.bookmarks = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Bookmark Paket'])
	]));

	var body = el('div', { class: 'eng-px' });
	ct.appendChild(body);

	function loadBookmarks() {
		showLoading(body);
		engRpc('get_bookmarks').then(function(r) {
			body.innerHTML = '';
			var bms = (r && r.bookmarks) || [];
			if (bms.length === 0) {
				body.appendChild(el('div', { class: 'eng-text-muted eng-text-center eng-mt-2' }, ['Tidak ada bookmark']));
				return;
			}
			bms.forEach(function(bm, i) {
				var name = bm.name || bm.package_name || '-';
				var oc = bm.option_code || bm.package_option_code || '-';
				var price = bm.price || bm.base_price || 0;
				body.appendChild(el('div', { class: 'eng-family-card' }, [
					el('div', { class: 'eng-family-info' }, [
						el('div', { class: 'eng-family-name' }, [name]),
						el('div', { class: 'eng-family-desc' }, [oc + ' \u2022 ' + fmtRp(price)])
					]),
					el('div', { class: 'eng-btn-group', style: { 'flex-direction': 'column' } }, [
						el('button', { class: 'eng-btn eng-btn-sm eng-btn-primary', onclick: function() { showPurchaseModal(bm); } }, ['Beli']),
						el('button', { class: 'eng-btn eng-btn-sm eng-btn-danger', 'data-idx': String(i), onclick: function() {
							var idx = parseInt(this.getAttribute('data-idx'));
							if (!confirm('Hapus bookmark?')) return;
							engRpc('delete_bookmark', { index: idx }).then(function() { loadBookmarks(); });
						} }, ['Hapus'])
					])
				]));
			});
		});
	}
	loadBookmarks();
};

/* Settings */
pages.settings = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('h1', { class: 'eng-page-title' }, ['Pengaturan'])
	]));

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	var sTabs = ['Custom Decoy', 'Saved Families', 'Auto Buy'];

	function renderSettingsTab(tab) {
		tabs.innerHTML = '';
		sTabs.forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t === tab ? ' active' : ''), onclick: function() { renderSettingsTab(t); } }, [t]));
		});
		body.innerHTML = '';
		if (tab === 'Custom Decoy') renderDecoy(body);
		else if (tab === 'Saved Families') renderSavedFamilies(body);
		else if (tab === 'Auto Buy') renderAutoBuy(body);
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderSettingsTab('Custom Decoy');
};

function renderDecoy(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Custom Decoy']));
	var body = el('div');
	card.appendChild(body);
	ct.appendChild(card);
	showLoading(body);
	engRpc('get_decoy').then(function(r) {
		body.innerHTML = '';
		body.appendChild(el('pre', { style: { color: 'var(--eng-text2)', 'font-size': '12px', 'white-space': 'pre-wrap' } }, [JSON.stringify(r || {}, null, 2)]));
	});
}

function renderSavedFamilies(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Saved Family Codes']));
	var body = el('div');
	card.appendChild(body);
	ct.appendChild(card);
	showLoading(body);
	engRpc('get_saved_families').then(function(r) {
		body.innerHTML = '';
		var fams = (r && r.families) || [];
		if (fams.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada family code tersimpan']));
			return;
		}
		fams.forEach(function(f) {
			body.appendChild(el('div', { class: 'eng-family-card' }, [
				el('div', { class: 'eng-family-info' }, [
					el('div', { class: 'eng-family-name' }, [f.code || f.family_code || '-']),
					el('div', { class: 'eng-family-desc' }, [f.name || '-'])
				])
			]));
		});
	});
}

function renderAutoBuy(ct) {
	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Auto Buy']));

	var statusDiv = el('div', { class: 'eng-mb-2' });
	var body = el('div');
	card.appendChild(statusDiv);
	card.appendChild(body);
	ct.appendChild(card);

	engRpc('autobuy_status').then(function(r) {
		var running = r && r.running;
		statusDiv.innerHTML = '';
		statusDiv.appendChild(el('div', { class: 'eng-flex-between' }, [
			el('span', {}, ['Status: ']),
			running ? el('span', { class: 'eng-badge eng-badge-success' }, ['RUNNING'])
			        : el('span', { class: 'eng-badge eng-badge-danger' }, ['STOPPED']),
			el('button', { class: running ? 'eng-btn eng-btn-sm eng-btn-danger' : 'eng-btn eng-btn-sm eng-btn-success', onclick: function() {
				engRpc('autobuy_control', { action: running ? 'stop' : 'start' }).then(function() { navigate('settings'); });
			} }, [running ? 'Stop' : 'Start'])
		]));
	});

	showLoading(body);
	engRpc('get_autobuy').then(function(r) {
		body.innerHTML = '';
		var entries = (r && r.entries) || [];
		if (entries.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada entri auto buy']));
			return;
		}
		body.appendChild(el('pre', { style: { color: 'var(--eng-text2)', 'font-size': '12px', 'white-space': 'pre-wrap' } }, [JSON.stringify(entries, null, 2)]));
	});
}

/* ── Main view ───────────────────────────────────── */
return view.extend({
	handleSaveApply: null,
	handleSave: null,
	handleReset: null,

	load: function() {
		var css = document.createElement('link');
		css.rel = 'stylesheet';
		css.href = L.resource('engsel/engsel.css');
		document.head.appendChild(css);
		return engRpc('auth_status');
	},

	render: function(authResult) {
		if (authResult && authResult.logged_in) {
			state.loggedIn = true;
			state.number = authResult.number || '';
			state.stype = authResult.subscription_type || '';
		}

		var container = el('div', { id: 'view-engsel' });
		var app = el('div', { class: 'eng-app' });

		/* Main content */
		mainContent = el('div', { class: 'eng-main' });
		app.appendChild(mainContent);

		/* Bottom tab bar (MyXL style) */
		var navbar = el('div', { class: 'eng-navbar' });
		NAV.forEach(function(n) {
			var item = el('div', {
				class: 'eng-nav-pill' + (n.id === state.page ? ' active' : ''),
				onclick: function() { navigate(n.id); }
			}, [
				el('span', { class: 'eng-nav-icon' }, [n.icon]),
				el('span', { class: 'eng-nav-label' }, [n.label])
			]);
			navItems[n.id] = item;
			navbar.appendChild(item);
		});
		app.appendChild(navbar);

		container.appendChild(app);

		renderPage();

		/* Auto refresh auth if logged in */
		if (state.loggedIn) {
			engRpc('refresh_auth').then(function(r) {
				if (r && r.balance != null) {
					state.balance = r.balance;
					state.expiredAt = r.expired_at || '--';
					var dashBal = document.getElementById('dash-balance');
					var dashExp = document.getElementById('dash-exp');
					if (dashBal) dashBal.textContent = fmtRp(state.balance);
					if (dashExp) dashExp.textContent = state.expiredAt;
				}
			});
			engRpc('notifications').then(function(r) {
				var notifs = (r && r.data && r.data.notifications) || (r && r.data) || [];
				if (Array.isArray(notifs)) state.notifCount = notifs.length;
			});
		}

		return container;
	}
});
