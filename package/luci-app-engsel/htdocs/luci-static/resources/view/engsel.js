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
	if (n == null || isNaN(n)) return 'Rp0';
	return 'Rp' + Number(n).toLocaleString('id-ID');
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

/* ── State ────────────────────────────────────────── */
var state = {
	page: 'beranda',
	accounts: [],
	activeIdx: 0,
	loggedIn: false,
	number: '',
	stype: '',
	balance: 0,
	expiredAt: '--',
	notifCount: 0,
	quotaData: null,
	quotaTotalData: '-',
	quotaTotalVoice: '-',
	quotaTotalSMS: '-'
};

var mainContent;
var navItems = {};

/* ── Bottom Tab Navigation (4 tabs like MyXL) ─────── */
var NAV = [
	{ id: 'beranda', icon: '\uD83C\uDFE0', label: 'Beranda' },
	{ id: 'store',   icon: '\uD83D\uDED2', label: 'XL Store' },
	{ id: 'tools',   icon: '\u2699\uFE0F', label: 'Tools' },
	{ id: 'profil',  icon: '\uD83D\uDC64', label: 'Profil' }
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
   BERANDA — Dashboard (MyXL Home Style)
   ═══════════════════════════════════════════════════ */
pages.beranda = function(ct) {
	/* Profile header */
	var profileHeader = el('div', { class: 'eng-profile-header' }, [
		el('div', { class: 'eng-profile-left' }, [
			el('div', { class: 'eng-avatar' }, ['\uD83D\uDC64']),
			el('div', {}, [
				el('div', { class: 'eng-profile-name' }, [state.loggedIn ? 'Engsel User' : 'Belum Login']),
				el('div', { class: 'eng-profile-number' }, [
					state.loggedIn ? String(state.number) : 'Login untuk memulai',
					state.loggedIn ? el('span', { style: { cursor: 'pointer', fontSize: '16px' } }, ['\u2795']) : null
				]),
				state.loggedIn ? el('div', { class: 'eng-profile-badge' }, [state.stype || 'PREPAID']) : null
			])
		]),
		el('div', { class: 'eng-notif-btn', id: 'eng-notif-bell', onclick: function() { navigate('tools'); } }, [
			'\uD83D\uDD14',
			state.notifCount > 0 ? el('span', { class: 'eng-notif-count', id: 'eng-notif-badge' }, [String(state.notifCount)]) : null
		])
	]);
	ct.appendChild(profileHeader);

	if (!state.loggedIn) {
		ct.appendChild(el('div', { class: 'eng-card' }, [
			el('div', { class: 'eng-card-title eng-mb-2' }, ['Selamat Datang di Engsel']),
			el('div', { class: 'eng-text-muted eng-mb-2' }, ['Login melalui tab Profil untuk mengakses semua fitur.']),
			el('button', { class: 'eng-btn eng-btn-primary eng-btn-block', onclick: function() { navigate('profil'); } }, ['Login Sekarang'])
		]));
		return;
	}

	/* Banner carousel */
	var bannerWrap = el('div', { class: 'eng-banner-wrap' });
	var bannerScroll = el('div', { class: 'eng-banner-scroll' });
	var banners = [
		{ tag: 'ENGSEL', title: 'MyXL Client', desc: 'Kelola paket, beli kuota, dan atur akun XL dari router OpenWrt.' },
		{ tag: 'PAKET', title: 'Beli Paket Cepat', desc: 'Pilih paket HOT atau cari berdasarkan option code & family code.' },
		{ tag: 'FITUR', title: 'Fitur Lengkap', desc: 'Circle, Transfer Pulsa, Family Plan, Auto Buy, dan lainnya.' }
	];
	banners.forEach(function(b) {
		bannerScroll.appendChild(el('div', { class: 'eng-banner-item' }, [
			el('div', { class: 'eng-banner-placeholder' }, [
				el('div', { class: 'eng-banner-tag' }, [b.tag]),
				el('div', { class: 'eng-banner-title' }, [b.title]),
				el('div', { class: 'eng-banner-desc' }, [b.desc])
			])
		]));
	});
	bannerWrap.appendChild(bannerScroll);
	ct.appendChild(bannerWrap);

	/* Quick Menu */
	ct.appendChild(el('div', { class: 'eng-section-header' }, [
		el('div', { class: 'eng-section-title' }, ['Menu Cepat']),
		el('div', { class: 'eng-section-link', onclick: function() { navigate('tools'); } }, ['Semua Menu'])
	]));
	var quickMenus = [
		{ icon: '\uD83D\uDCB0', label: 'Tagihan', page: 'history' },
		{ icon: '\u2295', label: 'Plan &\nBooster', page: 'packages' },
		{ icon: '\uD83D\uDCB3', label: 'Transfer\nPulsa', page: 'tools_transfer' },
		{ icon: '\u2B50', label: 'Promo &\nBookmark', page: 'bookmarks' }
	];
	var qg = el('div', { class: 'eng-quick-grid' });
	quickMenus.forEach(function(m) {
		qg.appendChild(el('div', { class: 'eng-quick-item', onclick: function() { navigate(m.page); } }, [
			el('div', { class: 'eng-quick-icon' }, [m.icon]),
			el('div', { class: 'eng-quick-label' }, [m.label])
		]));
	});
	ct.appendChild(qg);

	/* Quota Summary Card — clickable to packages */
	var quotaCard = el('div', { class: 'eng-quota-card', onclick: function() { navigate('packages'); } }, [
		el('div', { class: 'eng-quota-card-title' }, ['Lihat Paket Saya']),
		el('div', { class: 'eng-quota-summary' }, [
			el('div', {}, [
				el('div', { class: 'eng-quota-icon' }, ['\uD83C\uDF10']),
				el('div', { class: 'eng-quota-val', id: 'dash-data' }, [state.quotaTotalData]),
				el('div', { class: 'eng-quota-sub' }, ['Data'])
			]),
			el('div', {}, [
				el('div', { class: 'eng-quota-icon' }, ['\uD83D\uDCDE']),
				el('div', { class: 'eng-quota-val', id: 'dash-voice' }, [state.quotaTotalVoice]),
				el('div', { class: 'eng-quota-sub' }, ['Telepon'])
			]),
			el('div', {}, [
				el('div', { class: 'eng-quota-icon' }, ['\u2709\uFE0F']),
				el('div', { class: 'eng-quota-val', id: 'dash-sms' }, [state.quotaTotalSMS]),
				el('div', { class: 'eng-quota-sub' }, ['SMS'])
			])
		]),
		el('div', { class: 'eng-quota-link' }, [
			'Lihat Plan & Booster Saya',
			el('span', {}, ['\u203A'])
		])
	]);
	ct.appendChild(quotaCard);

	/* Balance Cards */
	var balanceRow = el('div', { class: 'eng-balance-row' }, [
		el('div', { class: 'eng-balance-card' }, [
			el('div', { class: 'eng-balance-label' }, ['Sisa Batas Pemakaian']),
			el('div', { class: 'eng-balance-value', id: 'dash-balance' }, [fmtRp(state.balance)]),
			el('div', { class: 'eng-balance-sub' }, ['Batas Pemakaian'])
		]),
		el('div', { class: 'eng-balance-card' }, [
			el('div', { class: 'eng-balance-label' }, ['Aktif Sampai']),
			el('div', { class: 'eng-balance-value', id: 'dash-exp' }, [state.expiredAt]),
			el('div', { class: 'eng-balance-sub' }, ['Masa Aktif'])
		])
	]);
	ct.appendChild(balanceRow);

	/* Fetch quota details async */
	engRpc('get_quota').then(function(r) {
		if (!r || r.error) return;
		var packages = (r.data && r.data.packages) || r.packages || [];
		if (!Array.isArray(packages)) packages = [];
		state.quotaData = packages;
		var totalData = 0, totalVoice = 0, totalSMS = 0;
		packages.forEach(function(pkg) {
			var bens = pkg.benefit || pkg.benefits || [];
			if (!Array.isArray(bens)) return;
			bens.forEach(function(b) {
				var rem = parseFloat(b.remaining_quota || b.remainingQuota || 0);
				var t = (b.type || '').toUpperCase();
				if (t === 'DATA' || t === 'INTERNET') totalData += rem;
				else if (t === 'VOICE' || t === 'CALL') totalVoice += rem;
				else if (t === 'SMS') totalSMS += rem;
			});
		});
		state.quotaTotalData = totalData > 0 ? (totalData / 1073741824).toFixed(2) + 'GB' : '-';
		state.quotaTotalVoice = totalVoice > 0 ? Math.round(totalVoice) + ' Min' : '-';
		state.quotaTotalSMS = totalSMS > 0 ? Math.round(totalSMS) + ' SMS' : '-';
		var dd = document.getElementById('dash-data');
		var dv = document.getElementById('dash-voice');
		var ds = document.getElementById('dash-sms');
		if (dd) dd.textContent = state.quotaTotalData;
		if (dv) dv.textContent = state.quotaTotalVoice;
		if (ds) ds.textContent = state.quotaTotalSMS;
	});
};

/* ═══════════════════════════════════════════════════
   PACKAGES — Plan & Booster (detail paket)
   ═══════════════════════════════════════════════════ */
pages.packages = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('beranda'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Plan & Booster Saya'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	var tabNames = ['Domestik', 'Roaming'];
	var activeTab = 'Domestik';

	function renderTab(tab) {
		activeTab = tab;
		tabs.innerHTML = '';
		tabNames.forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t === tab ? ' active' : ''), onclick: function() { renderTab(t); } }, [t]));
		});
		body.innerHTML = '';

		if (state.quotaData) {
			renderPackages(body, tab === 'Roaming');
		} else {
			showLoading(body);
			engRpc('get_quota').then(function(r) {
				body.innerHTML = '';
				if (!r || r.error) { showAlert(body, 'Gagal memuat paket: ' + (r ? r.error : 'Unknown'), 'danger'); return; }
				var packages = (r.data && r.data.packages) || r.packages || [];
				state.quotaData = Array.isArray(packages) ? packages : [];
				renderPackages(body, tab === 'Roaming');
			});
		}
	}

	function renderPackages(container, isRoaming) {
		var pkgs = (state.quotaData || []).filter(function(p) {
			var name = (p.package_name || p.packageName || '').toLowerCase();
			return isRoaming ? name.indexOf('roaming') >= 0 : name.indexOf('roaming') < 0;
		});
		if (pkgs.length === 0) {
			container.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada paket ' + (isRoaming ? 'roaming' : 'domestik')]));
			return;
		}
		container.appendChild(el('div', { class: 'eng-px eng-mt-2 eng-mb-1', style: { fontWeight: '700', fontSize: '16px' } }, ['Paket Utama']));
		pkgs.forEach(function(pkg) {
			var card = el('div', { class: 'eng-plan-card' });
			card.appendChild(el('div', { class: 'eng-plan-header' }, [
				el('div', { class: 'eng-plan-icon' }, ['\uD83C\uDF10']),
				el('div', { class: 'eng-plan-name' }, [pkg.package_name || pkg.packageName || 'Paket'])
			]));
			var bens = pkg.benefit || pkg.benefits || [];
			if (Array.isArray(bens)) {
				bens.forEach(function(b) {
					var rem = parseFloat(b.remaining_quota || b.remainingQuota || 0);
					var tot = parseFloat(b.total_quota || b.totalQuota || 0);
					var t = (b.type || '').toUpperCase();
					var displayRem = t === 'DATA' || t === 'INTERNET' ? (rem / 1073741824).toFixed(2) + ' GB' : rem;
					var displayTot = t === 'DATA' || t === 'INTERNET' ? (tot / 1073741824).toFixed(0) + ' GB' : String(tot);
					var pct = tot > 0 ? Math.min(100, (rem / tot) * 100) : 0;
					card.appendChild(el('div', { class: 'eng-plan-row' }, [
						el('div', { class: 'eng-plan-row-left' }, [
							el('span', { class: 'eng-plan-icon', style: { fontSize: '16px', marginRight: '6px' } }, ['\uD83C\uDF10']),
							b.description || b.name || t
						]),
						el('div', { class: 'eng-plan-row-value' }, [String(displayRem)])
					]));
					card.appendChild(el('div', { class: 'eng-quota-bar' }, [
						el('div', { class: 'eng-quota-fill', style: { width: pct + '%' } })
					]));
					card.appendChild(el('div', { class: 'eng-plan-total' }, [displayTot]));
				});
			}
			var resetDate = pkg.expired_at || pkg.expiredAt || pkg.reset_date || '';
			if (resetDate) {
				card.appendChild(el('div', { class: 'eng-plan-row eng-mt-1' }, [
					el('div', { class: 'eng-plan-row-left' }, ['\uD83D\uDCC5 Reset Kuota']),
					el('div', { class: 'eng-plan-row-value' }, [resetDate])
				]));
			}
			container.appendChild(card);
		});
		container.appendChild(el('div', { class: 'eng-px eng-mt-2 eng-mb-2' }, [
			el('button', { class: 'eng-btn eng-btn-dark eng-btn-block eng-btn-lg', onclick: function() { navigate('store'); } }, ['Tambah Booster'])
		]));
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderTab('Domestik');
};

/* ═══════════════════════════════════════════════════
   HISTORY — Riwayat/Tagihan
   ═══════════════════════════════════════════════════ */
pages.history = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('beranda'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Riwayat'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var body = el('div');
	ct.appendChild(body);
	showLoading(body);
	engRpc('transaction_history').then(function(r) {
		body.innerHTML = '';
		if (!r || r.error) { showAlert(body, 'Gagal: ' + (r ? r.error : 'Unknown'), 'danger'); return; }
		var items = (r.data && r.data.histories) || r.histories || (r.data && r.data) || [];
		if (!Array.isArray(items) || items.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Belum ada riwayat']));
			return;
		}
		items.forEach(function(h) {
			var card = el('div', { class: 'eng-card' }, [
				el('div', { class: 'eng-flex-between eng-mb-1' }, [
					el('div', { class: 'eng-card-title' }, [h.package_name || h.description || 'Transaksi']),
					el('span', { class: 'eng-badge eng-badge-' + (h.status === 'success' ? 'success' : 'warning') }, [h.status || 'pending'])
				]),
				el('div', { class: 'eng-text-sm eng-text-muted' }, [
					(h.date || h.created_at || '') + ' — ' + fmtRp(h.amount || h.price || 0)
				])
			]);
			body.appendChild(card);
		});
	});
};

/* ═══════════════════════════════════════════════════
   BOOKMARKS
   ═══════════════════════════════════════════════════ */
pages.bookmarks = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('beranda'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Bookmark Paket'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var body = el('div');
	ct.appendChild(body);
	showLoading(body);
	engRpc('get_bookmarks').then(function(r) {
		body.innerHTML = '';
		var bm = (r && r.bookmarks) || (r && r.data) || [];
		if (!Array.isArray(bm) || bm.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Belum ada bookmark']));
			return;
		}
		bm.forEach(function(b) {
			body.appendChild(el('div', { class: 'eng-family-card' }, [
				el('div', { class: 'eng-family-info' }, [
					el('div', { class: 'eng-family-name' }, [b.name || b.package_name || 'Paket']),
					el('div', { class: 'eng-family-desc' }, [
						'Code: ' + (b.option_code || b.code || '-') + ' | ' + fmtRp(b.price || 0)
					])
				]),
				el('div', { class: 'eng-family-icon' }, ['\u2B50'])
			]));
		});
	});
};

/* ═══════════════════════════════════════════════════
   XL STORE
   ═══════════════════════════════════════════════════ */
pages.store = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('div', { class: 'eng-page-title' }, ['XL Store'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	var storeTabs = [
		{ id: 'hot', label: 'HOT' },
		{ id: 'family', label: 'Tarif Dasar' },
		{ id: 'packages', label: 'Kuota' },
		{ id: 'redeem', label: 'Redeem' }
	];
	var activeTab = 'hot';

	function renderStoreTab(tab) {
		activeTab = tab;
		tabs.innerHTML = '';
		storeTabs.forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t.id === tab ? ' active' : ''), onclick: function() { renderStoreTab(t.id); } }, [t.label]));
		});
		body.innerHTML = '';
		if (tab === 'hot') renderStoreHot(body);
		else if (tab === 'family') renderStoreFamily(body);
		else if (tab === 'packages') renderStorePackages(body);
		else if (tab === 'redeem') renderStoreRedeem(body);
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderStoreTab('hot');
};

function renderStoreHot(ct) {
	showLoading(ct);
	engRpc('get_hot').then(function(r) {
		ct.innerHTML = '';
		var items = (r && r.packages) || (r && r.data) || [];
		if (!Array.isArray(items) || items.length === 0) {
			ct.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada paket HOT']));
			return;
		}
		var grid = el('div', { class: 'eng-card-grid eng-px eng-mt-2' });
		items.forEach(function(p) {
			grid.appendChild(el('div', { class: 'eng-pkg-card', onclick: function() { showPurchaseModal(p); } }, [
				el('div', { class: 'eng-pkg-name' }, [p.package_name || p.name || 'Paket']),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(p.price || 0)]),
				el('div', { class: 'eng-pkg-meta' }, [
					el('span', {}, ['Code: ' + (p.option_code || p.code || '-')]),
					p.validity ? el('span', {}, [p.validity]) : null
				])
			]));
		});
		ct.appendChild(grid);
	});
}

function renderStoreFamily(ct) {
	showLoading(ct);
	engRpc('store_family_list').then(function(r) {
		ct.innerHTML = '';
		var items = (r && r.data && r.data.family_list) || (r && r.data) || [];
		if (!Array.isArray(items) || items.length === 0) {
			ct.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada data']));
			return;
		}
		var wrap = el('div', { class: 'eng-px eng-mt-2' });
		items.forEach(function(f) {
			wrap.appendChild(el('div', { class: 'eng-family-card' }, [
				el('div', { class: 'eng-family-info' }, [
					el('div', { class: 'eng-family-name' }, [f.name || f.package_name || 'Plan']),
					el('div', { class: 'eng-family-desc' }, [f.description || fmtRp(f.price || 0)])
				]),
				el('div', { class: 'eng-family-icon' }, ['\uD83D\uDCCB'])
			]));
		});
		ct.appendChild(wrap);
	});
}

function renderStorePackages(ct) {
	showLoading(ct);
	engRpc('store_packages').then(function(r) {
		ct.innerHTML = '';
		var items = (r && r.data && r.data.packages) || (r && r.data) || [];
		if (!Array.isArray(items) || items.length === 0) {
			ct.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada data']));
			return;
		}
		var grid = el('div', { class: 'eng-card-grid eng-px eng-mt-2' });
		items.forEach(function(p) {
			grid.appendChild(el('div', { class: 'eng-pkg-card', onclick: function() { showPurchaseModal(p); } }, [
				el('div', { class: 'eng-pkg-name' }, [p.package_name || p.name || 'Paket']),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(p.price || 0)]),
				p.description ? el('div', { class: 'eng-pkg-meta' }, [el('span', {}, [p.description])]) : null
			]));
		});
		ct.appendChild(grid);
	});
}

function renderStoreRedeem(ct) {
	showLoading(ct);
	engRpc('store_redeemables').then(function(r) {
		ct.innerHTML = '';
		var items = (r && r.data && r.data.redeemables) || (r && r.data) || [];
		if (!Array.isArray(items) || items.length === 0) {
			ct.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada redeemable']));
			return;
		}
		var wrap = el('div', { class: 'eng-px eng-mt-2' });
		items.forEach(function(rd) {
			wrap.appendChild(el('div', { class: 'eng-family-card featured' }, [
				el('div', { class: 'eng-family-info' }, [
					el('div', { class: 'eng-family-badge' }, ['REDEEM']),
					el('div', { class: 'eng-family-name' }, [rd.name || rd.package_name || 'Item']),
					el('div', { class: 'eng-family-desc' }, [rd.description || ''])
				]),
				el('div', { class: 'eng-family-icon' }, ['\uD83C\uDF81'])
			]));
		});
		ct.appendChild(wrap);
	});
}

/* ── Purchase Modal ───────────────────────────────── */
function showPurchaseModal(pkg) {
	var overlay = el('div', { class: 'eng-modal-overlay', onclick: function(e) { if (e.target === overlay) overlay.remove(); } });
	var modal = el('div', { class: 'eng-modal' });
	modal.appendChild(el('h3', {}, [pkg.package_name || pkg.name || 'Paket']));

	var info = el('div', { class: 'eng-mb-2' });
	info.appendChild(el('div', { class: 'eng-flex-between eng-mb-1' }, [
		el('span', { class: 'eng-text-muted' }, ['Harga']),
		el('span', { style: { fontWeight: '700' } }, [fmtRp(pkg.price || 0)])
	]));
	if (pkg.option_code || pkg.code) {
		info.appendChild(el('div', { class: 'eng-flex-between eng-mb-1' }, [
			el('span', { class: 'eng-text-muted' }, ['Option Code']),
			el('span', {}, [pkg.option_code || pkg.code])
		]));
	}
	modal.appendChild(info);

	var statusMsg = el('div');
	modal.appendChild(statusMsg);

	var methods = [
		{ id: 'balance', label: 'Saldo/Pulsa', icon: '\uD83D\uDCB0' },
		{ id: 'ewallet', label: 'E-Wallet', icon: '\uD83D\uDCF1' },
		{ id: 'qris', label: 'QRIS', icon: '\uD83D\uDCF2' }
	];

	var btns = el('div', { class: 'eng-btn-group', style: { flexDirection: 'column' } });
	methods.forEach(function(m) {
		btns.appendChild(el('button', { class: 'eng-btn eng-btn-block', onclick: function() {
			var method = 'purchase_' + m.id;
			statusMsg.innerHTML = '';
			statusMsg.appendChild(el('div', { class: 'eng-loading' }, [el('div', { class: 'eng-spinner' }), 'Memproses...']));
			engRpc(method, {
				option_code: pkg.option_code || pkg.code || '',
				price: pkg.price || 0,
				name: pkg.package_name || pkg.name || '',
				token_confirmation: pkg.token_confirmation || pkg.confirmation_token || '',
				payment_for: pkg.payment_for || 'BUY_PACKAGE',
				overwrite_amount: pkg.overwrite_amount || 0
			}).then(function(r) {
				statusMsg.innerHTML = '';
				if (r && r.error) {
					statusMsg.appendChild(el('div', { class: 'eng-alert eng-alert-danger' }, ['Gagal: ' + r.error + (r.detail ? ' - ' + r.detail : '')]));
				} else {
					statusMsg.appendChild(el('div', { class: 'eng-alert eng-alert-success' }, ['Berhasil! ' + (r.message || '')]));
					if (r.qr_url || r.qris_url) {
						statusMsg.appendChild(el('div', { class: 'eng-text-center eng-mt-1' }, [
							el('img', { src: r.qr_url || r.qris_url, style: { maxWidth: '200px', margin: '0 auto' } })
						]));
					}
					state.quotaData = null;
				}
			});
		} }, [m.icon + ' ' + m.label]));
	});
	modal.appendChild(btns);

	modal.appendChild(el('button', { class: 'eng-btn eng-btn-block eng-mt-2', onclick: function() { overlay.remove(); } }, ['Tutup']));
	overlay.appendChild(modal);
	document.body.appendChild(overlay);
}

/* ═══════════════════════════════════════════════════
   TOOLS — All Advanced Features
   ═══════════════════════════════════════════════════ */
pages.tools = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('div', { class: 'eng-page-title' }, ['Tools & Fitur'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var menus = [
		{ icon: '\uD83D\uDCE6', title: 'Beli Paket', desc: 'HOT, Option Code, Family Code', page: 'buy' },
		{ icon: '\uD83D\uDD14', title: 'Notifikasi', desc: 'Lihat notifikasi terbaru', page: 'notif' },
		{ icon: '\uD83D\uDCB0', title: 'Riwayat Transaksi', desc: 'History pembelian', page: 'history' },
		{ icon: '\u2B50', title: 'Bookmark', desc: 'Paket yang disimpan', page: 'bookmarks' },
		{ icon: '\uD83D\uDD04', title: 'Circle', desc: 'Lihat circle & anggota', page: 'tools_circle' },
		{ icon: '\uD83D\uDCB3', title: 'Transfer Pulsa', desc: 'Kirim pulsa ke nomor lain', page: 'tools_transfer' },
		{ icon: '\uD83D\uDC68\u200D\uD83D\uDC69\u200D\uD83D\uDC67', title: 'Family Plan', desc: 'Info family plan', page: 'tools_famplan' },
		{ icon: '\uD83D\uDCDD', title: 'Registrasi Kartu', desc: 'Daftarkan NIK/KK', page: 'tools_register' },
		{ icon: '\uD83E\uDD16', title: 'Auto Buy', desc: 'Beli paket otomatis', page: 'tools_autobuy' },
		{ icon: '\uD83C\uDF81', title: 'Custom Decoy', desc: 'Paket custom sendiri', page: 'tools_decoy' },
		{ icon: '\u2699\uFE0F', title: 'Pengaturan', desc: 'Settings & konfigurasi', page: 'settings' }
	];

	var list = el('div', { class: 'eng-menu-list' });
	menus.forEach(function(m) {
		list.appendChild(el('div', { class: 'eng-menu-item', onclick: function() { navigate(m.page); } }, [
			el('div', { class: 'eng-menu-icon' }, [m.icon]),
			el('div', { class: 'eng-menu-text' }, [
				el('div', { class: 'eng-menu-title' }, [m.title]),
				el('div', { class: 'eng-menu-desc' }, [m.desc])
			]),
			el('div', { class: 'eng-menu-arrow' }, ['\u203A'])
		]));
	});
	ct.appendChild(list);
};

/* ── Tools sub-pages ──────────────────────────────── */

/* Buy page with tabs */
pages.buy = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Beli Paket'])
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
	showLoading(ct);
	engRpc('get_hot').then(function(r) {
		ct.innerHTML = '';
		var items = (r && r.packages) || (r && r.data) || [];
		if (!Array.isArray(items) || items.length === 0) {
			ct.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada paket HOT']));
			return;
		}
		var grid = el('div', { class: 'eng-card-grid eng-px eng-mt-2' });
		items.forEach(function(p) {
			grid.appendChild(el('div', { class: 'eng-pkg-card', onclick: function() { showPurchaseModal(p); } }, [
				el('div', { class: 'eng-pkg-name' }, [p.package_name || p.name || 'Paket']),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(p.price || 0)]),
				el('div', { class: 'eng-pkg-meta' }, [
					el('span', {}, ['Code: ' + (p.option_code || p.code || '-')])
				])
			]));
		});
		ct.appendChild(grid);
	});
}

function renderBuyOption(ct) {
	var form = el('div', { class: 'eng-card' });
	var input = el('input', { class: 'eng-input', type: 'text', placeholder: 'Masukkan option code (contoh: 41240)' });
	var results = el('div');
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-block eng-mt-1', onclick: function() {
		var code = input.value.trim();
		if (!code) return;
		showLoading(results);
		engRpc('get_package_detail', { option_code: code }).then(function(r) {
			results.innerHTML = '';
			var pkg = (r && r.data && r.data.package_detail) || (r && r.data) || r;
			if (pkg && pkg.error) { showAlert(results, 'Tidak ditemukan: ' + pkg.error, 'danger'); return; }
			showPurchaseModal({ package_name: pkg.package_name || pkg.name || code, price: pkg.price || pkg.base_price || 0, option_code: code, token_confirmation: pkg.token_confirmation || pkg.confirmation_token || '', payment_for: pkg.payment_for || 'BUY_PACKAGE' });
		});
	} }, ['Cari']);
	form.appendChild(el('div', { class: 'eng-form-group' }, [
		el('label', {}, ['Option Code']),
		input
	]));
	form.appendChild(btn);
	ct.appendChild(form);
	ct.appendChild(results);
}

function renderBuyFamily(ct) {
	var form = el('div', { class: 'eng-card' });
	var input = el('input', { class: 'eng-input', type: 'text', placeholder: 'Masukkan family code' });
	var results = el('div');
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-block eng-mt-1', onclick: function() {
		var code = input.value.trim();
		if (!code) return;
		showLoading(results);
		engRpc('family_bruteforce', { family_code: code }).then(function(r) {
			results.innerHTML = '';
			var pkgs = (r && r.data && r.data.package_variants) || (r && r.data) || [];
			if (!Array.isArray(pkgs)) pkgs = [pkgs];
			if (pkgs.length === 0) { showAlert(results, 'Tidak ditemukan', 'warning'); return; }
			pkgs.forEach(function(p) {
				results.appendChild(el('div', { class: 'eng-family-card', onclick: function() { showPurchaseModal(p); } }, [
					el('div', { class: 'eng-family-info' }, [
						el('div', { class: 'eng-family-name' }, [p.package_name || p.name || 'Paket']),
						el('div', { class: 'eng-family-desc' }, [fmtRp(p.price || 0)])
					])
				]));
			});
		});
	} }, ['Cari']);
	form.appendChild(el('div', { class: 'eng-form-group' }, [
		el('label', {}, ['Family Code']),
		input
	]));
	form.appendChild(btn);
	ct.appendChild(form);
	ct.appendChild(results);
}

function renderBuyLoop(ct) {
	ct.appendChild(el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-title eng-mb-2' }, ['Loop Family']),
		el('div', { class: 'eng-text-muted' }, ['Fitur Loop Family memungkinkan beli paket Loop melalui family code khusus. Gunakan tab "Family Code" untuk mencari.'])
	]));
}

/* Notifications */
pages.notif = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Notifikasi'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var body = el('div');
	ct.appendChild(body);
	showLoading(body);
	engRpc('notifications').then(function(r) {
		body.innerHTML = '';
		var notifs = (r && r.data && r.data.notifications) || (r && r.data) || [];
		if (!Array.isArray(notifs) || notifs.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada notifikasi']));
			return;
		}
		notifs.forEach(function(n) {
			body.appendChild(el('div', { class: 'eng-card' }, [
				el('div', { class: 'eng-card-title' }, [n.title || 'Notifikasi']),
				el('div', { class: 'eng-text-sm eng-text-muted eng-mt-1' }, [n.message || n.body || '']),
				n.date ? el('div', { class: 'eng-text-sm eng-text-muted eng-mt-1' }, [n.date]) : null
			]));
		});
	});
};

/* Circle */
pages.tools_circle = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Circle'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var body = el('div');
	ct.appendChild(body);
	showLoading(body);
	engRpc('circle_data').then(function(r) {
		body.innerHTML = '';
		if (r && r.error) { showAlert(body, 'Gagal: ' + r.error, 'danger'); return; }
		var data = r.data || r;
		body.appendChild(el('div', { class: 'eng-card' }, [
			el('div', { class: 'eng-card-title eng-mb-1' }, ['Circle Info']),
			el('pre', { style: { fontSize: '12px', overflow: 'auto', whiteSpace: 'pre-wrap', color: '#374151' } }, [JSON.stringify(data, null, 2)])
		]));
	});
};

/* Transfer */
pages.tools_transfer = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Transfer Pulsa'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var numInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Nomor tujuan (08xx)' });
	var amtInput = el('input', { class: 'eng-input', type: 'number', placeholder: 'Jumlah (Rp)' });
	var pinInput = el('input', { class: 'eng-input', type: 'password', placeholder: 'PIN Transfer' });
	var statusMsg = el('div');
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-block eng-btn-lg', onclick: function() {
		var num = numInput.value.trim(), amt = amtInput.value.trim(), pin = pinInput.value.trim();
		if (!num || !amt || !pin) { showAlert(statusMsg, 'Lengkapi semua field', 'warning'); return; }
		btn.disabled = true; btn.textContent = 'Memproses...';
		engRpc('transfer_balance', { receiver: num, amount: amt, pin: pin }).then(function(r) {
			btn.disabled = false; btn.textContent = 'Transfer';
			statusMsg.innerHTML = '';
			if (r && r.error) { showAlert(statusMsg, 'Gagal: ' + r.error, 'danger'); }
			else { showAlert(statusMsg, 'Transfer berhasil!', 'success'); }
		});
	} }, ['Transfer']);

	ct.appendChild(el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['Nomor Tujuan']), numInput]),
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['Jumlah']), amtInput]),
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['PIN']), pinInput]),
		btn,
		statusMsg
	]));
};

/* Family Plan */
pages.tools_famplan = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Family Plan'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var body = el('div');
	ct.appendChild(body);
	showLoading(body);
	engRpc('famplan_info').then(function(r) {
		body.innerHTML = '';
		if (r && r.error) { showAlert(body, 'Gagal: ' + r.error, 'danger'); return; }
		var data = r.data || r;
		body.appendChild(el('div', { class: 'eng-card' }, [
			el('div', { class: 'eng-card-title eng-mb-1' }, ['Family Plan Info']),
			el('pre', { style: { fontSize: '12px', overflow: 'auto', whiteSpace: 'pre-wrap', color: '#374151' } }, [JSON.stringify(data, null, 2)])
		]));
	});
};

/* Register Card */
pages.tools_register = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Registrasi Kartu'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var msisdnInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Nomor HP (08xx)', value: state.number || '' });
	var nikInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'NIK (16 digit)' });
	var kkInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'No. KK (16 digit)' });
	var statusMsg = el('div');
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-block eng-btn-lg', onclick: function() {
		var msisdn = msisdnInput.value.trim(), nik = nikInput.value.trim(), kk = kkInput.value.trim();
		if (!msisdn || !nik || !kk) { showAlert(statusMsg, 'Lengkapi semua field', 'warning'); return; }
		btn.disabled = true; btn.textContent = 'Memproses...';
		engRpc('register_card', { msisdn: msisdn, nik: nik, kk: kk }).then(function(r) {
			btn.disabled = false; btn.textContent = 'Daftarkan';
			statusMsg.innerHTML = '';
			if (r && r.error) { showAlert(statusMsg, 'Gagal: ' + r.error, 'danger'); }
			else { showAlert(statusMsg, 'Registrasi berhasil!', 'success'); }
		});
	} }, ['Daftarkan']);

	ct.appendChild(el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['Nomor HP']), msisdnInput]),
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['NIK']), nikInput]),
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['No. KK']), kkInput]),
		btn,
		statusMsg
	]));
};

/* Auto Buy */
pages.tools_autobuy = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Auto Buy'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var codeInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Option code paket' });
	var intervalInput = el('input', { class: 'eng-input', type: 'number', placeholder: 'Interval (detik)', value: '86400' });
	var statusMsg = el('div');
	ct.appendChild(el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-title eng-mb-2' }, ['Auto Buy']),
		el('div', { class: 'eng-text-muted eng-mb-2' }, ['Otomatis beli paket setiap interval tertentu. Fitur ini dijalankan di CLI.']),
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['Option Code']), codeInput]),
		el('div', { class: 'eng-form-group' }, [el('label', {}, ['Interval (detik)']), intervalInput]),
		el('div', { class: 'eng-alert eng-alert-info' }, ['Fitur Auto Buy menggunakan cron job di CLI. Konfigurasi di /etc/engsel/autobuy.conf']),
		statusMsg
	]));
};

/* Custom Decoy */
pages.tools_decoy = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Custom Decoy'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var body = el('div');
	ct.appendChild(body);
	showLoading(body);
	engRpc('get_decoy').then(function(r) {
		body.innerHTML = '';
		var items = (r && r.packages) || (r && r.data) || [];
		if (!Array.isArray(items) || items.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-center eng-text-muted eng-mt-2' }, ['Tidak ada custom decoy']));
			return;
		}
		var grid = el('div', { class: 'eng-card-grid eng-px' });
		items.forEach(function(p) {
			grid.appendChild(el('div', { class: 'eng-pkg-card', onclick: function() { showPurchaseModal(p); } }, [
				el('div', { class: 'eng-pkg-name' }, [p.package_name || p.name || 'Custom']),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(p.price || 0)]),
				el('div', { class: 'eng-pkg-meta' }, [el('span', {}, ['Code: ' + (p.option_code || p.code || '-')])])
			]));
		});
		body.appendChild(grid);
	});
};

/* Settings */
pages.settings = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('button', { class: 'eng-back-btn', onclick: function() { navigate('tools'); } }, ['\u2190']),
		el('div', { class: 'eng-page-title' }, ['Pengaturan'])
	]));

	ct.appendChild(el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-title eng-mb-1' }, ['Tentang']),
		el('div', { class: 'eng-text-muted' }, ['Engsel - MyXL Client for OpenWrt']),
		el('div', { class: 'eng-text-muted eng-mt-1' }, ['Data disimpan di /etc/engsel/']),
		el('div', { class: 'eng-text-muted eng-mt-1' }, ['LuCI app membaca data langsung dari file CLI'])
	]));

	ct.appendChild(el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-title eng-mb-1' }, ['Sinkronisasi']),
		el('div', { class: 'eng-text-muted eng-mb-2' }, ['LuCI app menggunakan file yang sama dengan CLI (refresh-tokens.json, hot.json, bookmark.json)']),
		el('button', { class: 'eng-btn eng-btn-primary eng-btn-block', onclick: function() {
			state.quotaData = null;
			navigate('beranda');
		} }, ['Refresh Data'])
	]));
};

/* ═══════════════════════════════════════════════════
   PROFIL — Account Management
   ═══════════════════════════════════════════════════ */
pages.profil = function(ct) {
	ct.appendChild(el('div', { class: 'eng-page-header' }, [
		el('div', { class: 'eng-page-title' }, ['Profil & Akun'])
	]));

	/* Login form */
	var loginCard = el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-title eng-mb-2' }, ['Login / Tambah Akun'])
	]);

	var loginForm = el('div');
	var numInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Nomor HP (08xx / 628xx)' });
	var otpInput = el('input', { class: 'eng-input eng-mt-1', type: 'text', placeholder: 'Kode OTP', style: { display: 'none' } });
	var loginMsg = el('div', { class: 'eng-mt-1' });
	var btnOtp = el('button', { class: 'eng-btn eng-btn-primary eng-btn-block eng-btn-lg eng-mt-1' }, ['Kirim OTP']);
	var btnSubmit = el('button', { class: 'eng-btn eng-btn-success eng-btn-block eng-btn-lg eng-mt-1', style: { display: 'none' } }, ['Verifikasi']);
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
			loginMsg.textContent = 'Login berhasil!';
			loginMsg.className = 'eng-mt-1 eng-alert eng-alert-success';
			loadAccounts();
			setTimeout(function() { navigate('beranda'); }, 1000);
		});
	});

	loginForm.appendChild(el('div', { class: 'eng-form-group' }, [el('label', {}, ['Nomor HP']), numInput]));
	loginForm.appendChild(otpInput);
	loginForm.appendChild(loginMsg);
	loginForm.appendChild(btnOtp);
	loginForm.appendChild(btnSubmit);
	loginCard.appendChild(loginForm);
	ct.appendChild(loginCard);

	/* Account list */
	var accountSection = el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-title eng-mb-2' }, ['Daftar Akun'])
	]);
	var accountList = el('div', { id: 'eng-account-list' });
	accountSection.appendChild(accountList);
	ct.appendChild(accountSection);

	renderAccountList(accountList);
};

function renderAccountList(container) {
	container.innerHTML = '';
	if (state.accounts.length === 0) {
		container.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Belum ada akun. Login untuk menambahkan.']));
		return;
	}
	state.accounts.forEach(function(acc, idx) {
		var isActive = idx === state.activeIdx;
		var card = el('div', { class: 'eng-account-card' + (isActive ? ' active' : '') }, [
			el('div', { class: 'eng-account-info' }, [
				el('div', { class: 'eng-account-num' }, [String(acc.number || acc.msisdn || 'Unknown')]),
				el('div', { class: 'eng-account-type' }, [
					(acc.subscription_type || acc.stype || 'PREPAID'),
					isActive ? ' — Aktif' : ''
				])
			]),
			el('div', { class: 'eng-btn-group' }, [
				!isActive ? el('button', { class: 'eng-btn eng-btn-sm eng-btn-primary', onclick: function(e) {
					e.stopPropagation();
					engRpc('switch_account', { index: idx }).then(function() {
						state.activeIdx = idx;
						loadAccounts();
						navigate('profil');
					});
				} }, ['Pilih']) : null,
				el('button', { class: 'eng-btn eng-btn-sm eng-btn-danger', onclick: function(e) {
					e.stopPropagation();
					if (confirm('Hapus akun ' + (acc.number || '') + '?')) {
						engRpc('delete_account', { index: idx }).then(function() {
							loadAccounts();
							navigate('profil');
						});
					}
				} }, ['\u2715'])
			])
		]);
		container.appendChild(card);
	});
}

function loadAccounts() {
	engRpc('list_accounts').then(function(r) {
		var list = (r && r.accounts) || (r && r.data) || [];
		state.accounts = Array.isArray(list) ? list : [];
		if (state.accounts.length > 0) {
			var active = state.accounts[state.activeIdx] || state.accounts[0];
			state.loggedIn = true;
			state.number = active.number || active.msisdn || '';
			state.stype = active.subscription_type || active.stype || 'PREPAID';
		} else {
			state.loggedIn = false;
			state.number = '';
			state.stype = '';
		}
		var al = document.getElementById('eng-account-list');
		if (al) renderAccountList(al);
	});
}

/* ═══════════════════════════════════════════════════
   LuCI View
   ═══════════════════════════════════════════════════ */
return view.extend({
	title: 'Engsel',
	handleSaveApply: null,
	handleSave: null,
	handleReset: null,

	load: function() {
		var css = document.createElement('link');
		css.rel = 'stylesheet';
		css.href = L.resource('engsel/engsel.css');
		document.head.appendChild(css);
	},

	render: function() {
		var container = el('div', { class: 'eng-app', id: 'view-engsel' });
		mainContent = el('div', { class: 'eng-main' });
		container.appendChild(mainContent);

		/* Bottom navigation bar */
		var navbar = el('div', { class: 'eng-navbar' });
		NAV.forEach(function(item) {
			var pill = el('div', {
				class: 'eng-nav-pill' + (item.id === state.page ? ' active' : ''),
				onclick: function() { navigate(item.id); }
			}, [
				el('div', { class: 'eng-nav-icon' }, [item.icon]),
				el('div', { class: 'eng-nav-label' }, [item.label])
			]);
			navItems[item.id] = pill;
			navbar.appendChild(pill);
		});
		container.appendChild(navbar);

		/* Load accounts and initial data */
		loadAccounts();

		engRpc('auth_status').then(function(r) {
			if (r && r.logged_in) {
				state.loggedIn = true;
				state.number = r.number || state.number;
				state.stype = r.subscription_type || state.stype;
			}
			renderPage();

			if (state.loggedIn) {
				engRpc('get_balance').then(function(r) {
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
					if (Array.isArray(notifs)) {
						state.notifCount = notifs.length;
						var bell = document.getElementById('eng-notif-bell');
						if (bell && state.notifCount > 0) {
							var existing = document.getElementById('eng-notif-badge');
							if (existing) {
								existing.textContent = String(state.notifCount);
							} else {
								bell.appendChild(el('span', { class: 'eng-notif-count', id: 'eng-notif-badge' }, [String(state.notifCount)]));
							}
						}
					}
				});
			}
		});

		return container;
	}
});
