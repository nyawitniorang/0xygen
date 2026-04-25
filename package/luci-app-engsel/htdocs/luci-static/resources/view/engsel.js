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
	page: 'dashboard',
	accounts: [],
	activeIdx: 0,
	loggedIn: false,
	number: '',
	stype: '',
	balance: 0,
	expiredAt: '--'
};

var mainContent;
var navItems = {};

/* ── Navigation ──────────────────────────────────── */
var NAV = [
	{ id: 'dashboard',  icon: '\uD83C\uDFE0', label: 'Dashboard' },
	{ id: 'accounts',   icon: '\uD83D\uDC64', label: 'Akun' },
	{ id: 'packages',   icon: '\uD83D\uDCE6', label: 'Paket' },
	{ id: 'buy',        icon: '\uD83D\uDED2', label: 'Beli' },
	{ id: 'store',      icon: '\uD83C\uDFEA', label: 'Store' },
	{ id: 'history',    icon: '\uD83D\uDCCB', label: 'Riwayat' },
	{ id: 'features',   icon: '\u2699\uFE0F', label: 'Fitur' },
	{ id: 'notif',      icon: '\uD83D\uDD14', label: 'Notif' },
	{ id: 'register',   icon: '\uD83D\uDCDD', label: 'Registrasi' },
	{ id: 'bookmarks',  icon: '\u2B50',        label: 'Bookmark' },
	{ id: 'settings',   icon: '\uD83D\uDD27', label: 'Setting' }
];

function navigate(page) {
	state.page = page;
	Object.keys(navItems).forEach(function(k) {
		navItems[k].classList.toggle('active', k === page);
	});
	renderPage();
	/* scroll active pill into view */
	if (navItems[page]) navItems[page].scrollIntoView({ behavior: 'smooth', inline: 'center', block: 'nearest' });
}

function renderPage() {
	if (!mainContent) return;
	mainContent.innerHTML = '';
	var renderFn = pages[state.page];
	if (renderFn) renderFn(mainContent);
	else mainContent.appendChild(el('div', {}, ['Halaman tidak ditemukan']));
}

/* ── Pages ───────────────────────────────────────── */
var pages = {};

/* Dashboard */
pages.dashboard = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Dashboard'])
	]));

	var grid = el('div', { class: 'eng-card-grid' });

	var numCard = el('div', { class: 'eng-stat-card' }, [
		el('div', { class: 'eng-stat-label' }, ['Nomor Aktif']),
		el('div', { class: 'eng-stat-value', id: 'dash-number' }, [state.loggedIn ? state.number : 'BELUM LOGIN']),
		el('div', { class: 'eng-stat-sub' }, ['Tipe: ' + (state.stype || 'N/A')])
	]);

	var balCard = el('div', { class: 'eng-stat-card accent-green' }, [
		el('div', { class: 'eng-stat-label' }, ['Saldo Pulsa']),
		el('div', { class: 'eng-stat-value', id: 'dash-balance' }, [fmtRp(state.balance)]),
		el('div', { class: 'eng-stat-sub', id: 'dash-exp' }, ['Aktif sampai: ' + state.expiredAt])
	]);

	var ptCard = el('div', { class: 'eng-stat-card accent-blue' }, [
		el('div', { class: 'eng-stat-label' }, ['Points']),
		el('div', { class: 'eng-stat-value' }, ['N/A']),
		el('div', { class: 'eng-stat-sub' }, ['Tier: N/A'])
	]);

	grid.appendChild(numCard);
	grid.appendChild(balCard);
	grid.appendChild(ptCard);
	ct.appendChild(grid);

	/* Quick actions */
	var actions = el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-header' }, [
			el('h3', { class: 'eng-card-title' }, ['Aksi Cepat'])
		]),
		el('div', { class: 'eng-btn-group' }, [
			el('button', { class: 'eng-btn eng-btn-primary', onclick: function() { navigate('buy'); } }, ['Beli Paket']),
			el('button', { class: 'eng-btn', onclick: function() { navigate('packages'); } }, ['Lihat Paket']),
			el('button', { class: 'eng-btn', onclick: function() { navigate('history'); } }, ['Riwayat']),
			el('button', { class: 'eng-btn', onclick: function() { navigate('store'); } }, ['XL Store'])
		])
	]);
	ct.appendChild(actions);

	/* Active packages preview */
	var pkgCard = el('div', { class: 'eng-card' }, [
		el('div', { class: 'eng-card-header' }, [
			el('h3', { class: 'eng-card-title' }, ['Paket Aktif']),
			el('button', { class: 'eng-btn eng-btn-sm', onclick: function() { navigate('packages'); } }, ['Lihat Semua'])
		])
	]);
	var pkgBody = el('div', { id: 'dash-packages' });
	pkgCard.appendChild(pkgBody);
	ct.appendChild(pkgCard);

	if (state.loggedIn) loadDashboard(pkgBody);
	else showAlert(ct, 'Silakan login terlebih dahulu', 'warning');
};

function loadDashboard(pkgContainer) {
	showLoading(pkgContainer);
	engRpc('get_quota').then(function(r) {
		pkgContainer.innerHTML = '';
		var quotas = (r && r.data && r.data.quotas) || (r && r.data) || [];
		if (!Array.isArray(quotas)) quotas = [];
		if (quotas.length === 0) {
			pkgContainer.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada paket aktif']));
			return;
		}
		var table = el('table', { class: 'eng-table' });
		table.appendChild(el('thead', {}, [el('tr', {}, [
			el('th', {}, ['Nama']),
			el('th', {}, ['Kuota']),
			el('th', {}, ['Sisa']),
			el('th', {}, ['Exp'])
		])]));
		var tbody = el('tbody');
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
			tbody.appendChild(el('tr', {}, [
				el('td', {}, [name]),
				el('td', {}, [total ? fmtBytes(parseFloat(total)) : '-']),
				el('td', {}, [
					el('div', {}, [remain ? fmtBytes(parseFloat(remain)) : '-']),
					el('div', { class: 'eng-quota-bar' }, [
						el('div', { class: 'eng-quota-fill', style: { width: pct + '%' } })
					])
				]),
				el('td', { class: 'eng-text-sm eng-text-muted' }, [exp])
			]));
		});
		table.appendChild(tbody);
		pkgContainer.appendChild(table);
	});
}

/* Accounts */
pages.accounts = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Manajemen Akun'])
	]));

	/* Login form */
	var loginCard = el('div', { class: 'eng-card' }, [
		el('h3', { class: 'eng-card-title eng-mb-2' }, ['Login / Tambah Akun'])
	]);

	var loginForm = el('div');
	var numInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Nomor HP (08xx / 628xx)', id: 'login-number' });
	var otpInput = el('input', { class: 'eng-input', type: 'text', placeholder: 'Kode OTP', id: 'login-otp', style: { display: 'none' } });
	var loginMsg = el('div', { class: 'eng-mt-1', id: 'login-msg' });
	var btnOtp = el('button', { class: 'eng-btn eng-btn-primary eng-mt-1', id: 'btn-otp' }, ['Kirim OTP']);
	var btnSubmit = el('button', { class: 'eng-btn eng-btn-success eng-mt-1', id: 'btn-submit-otp', style: { display: 'none' } }, ['Verifikasi']);
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

	loginForm.appendChild(el('div', { class: 'eng-form-group' }, [
		el('label', {}, ['Nomor HP']),
		numInput
	]));
	loginForm.appendChild(el('div', { class: 'eng-form-group' }, [
		el('label', {}, ['Kode OTP']),
		otpInput
	]));
	loginForm.appendChild(el('div', { class: 'eng-btn-group' }, [btnOtp, btnSubmit]));
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
	var accountList = el('div', { id: 'account-list' });
	accountCard.appendChild(accountList);
	ct.appendChild(accountCard);

	refreshAccounts(accountList);
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
		var table = el('table', { class: 'eng-table' });
		table.appendChild(el('thead', {}, [el('tr', {}, [
			el('th', {}, ['#']),
			el('th', {}, ['Nomor']),
			el('th', {}, ['Tipe']),
			el('th', {}, ['Status']),
			el('th', {}, ['Aksi'])
		])]));
		var tbody = el('tbody');
		accounts.forEach(function(acc, i) {
			var isActive = acc.active || (i === state.activeIdx);
			tbody.appendChild(el('tr', {}, [
				el('td', {}, [String(i + 1)]),
				el('td', {}, [String(acc.number || '-')]),
				el('td', {}, [
					el('span', { class: 'eng-badge eng-badge-info' }, [acc.subscription_type || 'N/A'])
				]),
				el('td', {}, [
					isActive ? el('span', { class: 'eng-badge eng-badge-success' }, ['AKTIF'])
					         : el('span', { class: 'eng-badge eng-badge-purple' }, ['IDLE'])
				]),
				el('td', {}, [
					el('div', { class: 'eng-btn-group' }, [
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
							engRpc('delete_account', { index: idx }).then(function() {
								refreshAccounts(container);
							});
						} }, ['Hapus'])
					])
				])
			]));
		});
		table.appendChild(tbody);
		container.appendChild(table);
	});
}

/* My Packages */
pages.packages = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Paket Saya'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }
	var card = el('div', { class: 'eng-card' });
	var body = el('div');
	card.appendChild(body);
	ct.appendChild(card);
	showLoading(body);

	engRpc('get_quota').then(function(r) {
		body.innerHTML = '';
		var quotas = [];
		if (r && r.data) {
			quotas = r.data.quotas || r.data.packages || r.data;
			if (!Array.isArray(quotas)) quotas = [quotas];
		}
		if (quotas.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada paket aktif']));
			return;
		}
		var table = el('table', { class: 'eng-table' });
		table.appendChild(el('thead', {}, [el('tr', {}, [
			el('th', {}, ['Nama']),
			el('th', {}, ['Kuota Total']),
			el('th', {}, ['Sisa']),
			el('th', {}, ['Expired']),
			el('th', {}, ['Tipe']),
			el('th', {}, ['Aksi'])
		])]));
		var tbody = el('tbody');
		quotas.forEach(function(q) {
			var name = q.name || q.quota_name || q.product_name || '-';
			var total = q.total || q.quota_total || '';
			var remain = q.remaining || q.quota_remaining || '';
			var exp = q.expired_at || q.expiry_date || '-';
			if (typeof exp === 'string' && exp.length > 10) exp = exp.substring(0, 10);
			var stype = q.product_subscription_type || q.subscription_type || '-';
			var domain = q.product_domain || '-';
			var qcode = q.quota_code || q.code || '';
			var pct = 0;
			if (total && remain) {
				var t = parseFloat(total), rm = parseFloat(remain);
				if (t > 0) pct = Math.round((rm / t) * 100);
			}
			tbody.appendChild(el('tr', {}, [
				el('td', {}, [
					el('div', { class: 'eng-pkg-name' }, [name]),
					el('div', { class: 'eng-text-sm eng-text-muted' }, ['Code: ' + qcode])
				]),
				el('td', {}, [total ? fmtBytes(parseFloat(total)) : '-']),
				el('td', {}, [
					el('div', {}, [remain ? fmtBytes(parseFloat(remain)) : '-']),
					el('div', { class: 'eng-quota-bar' }, [
						el('div', { class: 'eng-quota-fill', style: { width: pct + '%' } })
					])
				]),
				el('td', { class: 'eng-text-sm' }, [exp]),
				el('td', {}, [el('span', { class: 'eng-badge eng-badge-info' }, [stype])]),
				el('td', {}, [
					qcode ? el('button', { class: 'eng-btn eng-btn-sm eng-btn-danger', 'data-qc': qcode, 'data-st': stype, 'data-dm': domain, onclick: function() {
						if (!confirm('Unsub paket ini?')) return;
						engRpc('unsubscribe', {
							quota_code: this.getAttribute('data-qc'),
							product_subscription_type: this.getAttribute('data-st'),
							product_domain: this.getAttribute('data-dm')
						}).then(function(ur) {
							showAlert(body, JSON.stringify(ur), ur.error ? 'danger' : 'success');
							navigate('packages');
						});
					} }, ['Unsub']) : null
				])
			]));
		});
		table.appendChild(tbody);
		body.appendChild(table);
	});
};

/* Buy Package */
pages.buy = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Beli Paket'])
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
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Paket HOT']));
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
			var pkgEl = el('div', { class: 'eng-pkg-card', onclick: function() { showPurchaseModal(p); } }, [
				el('div', { class: 'eng-pkg-name' }, [p.name || p.option_code || '-']),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(p.price || 0)]),
				el('div', { class: 'eng-pkg-meta' }, [
					el('span', {}, ['Code: ' + (p.option_code || '-')]),
					p.family_code ? el('span', {}, ['Family: ' + p.family_code]) : null
				])
			]);
			grid.appendChild(pkgEl);
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

			resultDiv.appendChild(el('div', { class: 'eng-card' }, [
				el('div', { class: 'eng-pkg-name' }, [name]),
				el('div', { class: 'eng-pkg-price' }, [fmtRp(price)]),
				desc ? el('div', { class: 'eng-text-sm eng-text-muted eng-mt-1' }, [desc]) : null,
				el('div', { class: 'eng-mt-2' }, [
					el('button', { class: 'eng-btn eng-btn-primary', onclick: function() {
						showPurchaseModal({ option_code: oc, name: name, price: price, token_confirmation: conf, payment_for: payFor });
					} }, ['Beli Paket Ini'])
				])
			]));
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [
		el('label', {}, ['Option Code']),
		optInput
	]));
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
					el('div', { class: 'eng-pkg-meta' }, [
						el('span', {}, ['Code: ' + oc])
					])
				]));
			});
			resultDiv.appendChild(grid);
		});
	});

	card.appendChild(el('div', { class: 'eng-form-group' }, [
		el('label', {}, ['Family Code']),
		famInput
	]));
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
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-lg eng-mt-1' }, ['Mulai Loop Pembelian']);
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
				running = false;
				btn.disabled = false;
				btn.textContent = 'Mulai Loop Pembelian';
				return;
			}
			var variants = (r && r.data && r.data.package_variants) || [];
			if (variants.length === 0) {
				showAlert(resultDiv, 'Tidak ada paket', 'warning');
				running = false;
				btn.disabled = false;
				btn.textContent = 'Mulai Loop Pembelian';
				return;
			}
			btn.textContent = 'Membeli ' + variants.length + ' paket...';
			var delay = parseInt(delayInput.value) || 2;
			var logEl = el('div', { class: 'eng-card', style: { 'max-height': '400px', 'overflow-y': 'auto' } });
			resultDiv.appendChild(logEl);

			function buyNext(idx) {
				if (idx >= variants.length) {
					logEl.appendChild(el('div', { class: 'eng-alert eng-alert-success' }, ['Selesai! ' + variants.length + ' paket diproses.']));
					running = false;
					btn.disabled = false;
					btn.textContent = 'Mulai Loop Pembelian';
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

	var extraFields = el('div', { class: 'eng-mt-2', id: 'purchase-extra' });
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

	var btnBuy = el('button', { class: 'eng-btn eng-btn-primary eng-btn-lg eng-w-full' }, ['Beli Sekarang']);
	var btnCancel = el('button', { class: 'eng-btn eng-w-full eng-mt-1' }, ['Batal']);

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
					btnBuy.disabled = false;
					btnBuy.textContent = 'Beli Sekarang';
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
				btnBuy.disabled = false;
				btnBuy.textContent = 'Beli Sekarang';
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

/* XL Store */
pages.store = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['XL Store'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var tabs = el('div', { class: 'eng-tabs' });
	var body = el('div');
	var storeTabs = ['Family List', 'Packages', 'Segments', 'Redeemables'];

	function renderStoreTab(tab) {
		tabs.innerHTML = '';
		storeTabs.forEach(function(t) {
			tabs.appendChild(el('button', { class: 'eng-tab' + (t === tab ? ' active' : ''), onclick: function() { renderStoreTab(t); } }, [t]));
		});
		body.innerHTML = '';
		showLoading(body);
		var rpcMethod = tab === 'Family List' ? 'store_family_list' :
		               tab === 'Packages' ? 'store_packages' :
		               tab === 'Segments' ? 'store_segments' : 'store_redeemables';
		engRpc(rpcMethod).then(function(r) {
			body.innerHTML = '';
			if (r && r.error) { showAlert(body, 'Error: ' + r.error, 'danger'); return; }
			var data = (r && r.data) || r || {};
			var items = data.families || data.packages || data.segments || data.redeemables || [];
			if (!Array.isArray(items)) items = [items];
			if (items.length === 0) {
				body.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada data']));
				return;
			}
			var grid = el('div', { class: 'eng-card-grid' });
			items.forEach(function(item) {
				var name = item.name || item.family_name || item.segment_name || '-';
				var code = item.code || item.family_code || item.option_code || '';
				var price = item.price || item.base_price || null;
				var desc = item.description || '';
				grid.appendChild(el('div', { class: 'eng-pkg-card', onclick: function() {
					if (code && price != null) {
						showPurchaseModal({ option_code: code, name: name, price: price, token_confirmation: item.token_confirmation || '', payment_for: item.payment_for || 'BUY_PACKAGE' });
					} else if (item.family_code) {
						navigate('buy');
					}
				} }, [
					el('div', { class: 'eng-pkg-name' }, [name]),
					price != null ? el('div', { class: 'eng-pkg-price' }, [fmtRp(price)]) : null,
					el('div', { class: 'eng-pkg-meta' }, [
						code ? el('span', {}, ['Code: ' + code]) : null,
						desc ? el('span', {}, [desc.substring(0, 50)]) : null
					])
				]));
			});
			body.appendChild(grid);
		});
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderStoreTab('Family List');
};

/* Transaction History */
pages.history = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Riwayat Transaksi'])
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
				body.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada data']));
				return;
			}
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
			body.appendChild(el('div', { class: 'eng-card' }, [table]));
		});
	}

	ct.appendChild(tabs);
	ct.appendChild(body);
	renderHistoryTab('Riwayat');
};

/* Advanced Features */
pages.features = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Fitur Lanjutan'])
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
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-lg' }, ['Transfer']);

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
	var btn = el('button', { class: 'eng-btn eng-btn-primary' }, ['Validate']);

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

/* Notifications */
pages.notif = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Notifikasi'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var card = el('div', { class: 'eng-card' });
	var body = el('div');
	card.appendChild(body);
	ct.appendChild(card);
	showLoading(body);

	engRpc('notifications').then(function(r) {
		body.innerHTML = '';
		if (r && r.error) { showAlert(body, 'Error: ' + r.error, 'danger'); return; }
		var notifs = (r && r.data && r.data.notifications) || (r && r.data) || [];
		if (!Array.isArray(notifs)) notifs = [notifs];
		if (notifs.length === 0) {
			body.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada notifikasi']));
			return;
		}
		notifs.forEach(function(n) {
			var title = n.title || n.name || '-';
			var msg = n.message || n.body || n.description || '';
			var date = n.date || n.created_at || '';
			body.appendChild(el('div', { class: 'eng-card', style: { cursor: 'pointer' }, onclick: function() {
				if (n.id || n.notification_id) {
					engRpc('notification_detail', { notification_id: n.id || n.notification_id }).then(function(d) {
						var detail = (d && d.data) || d || {};
						alert(JSON.stringify(detail, null, 2));
					});
				}
			} }, [
				el('div', { class: 'eng-flex-between' }, [
					el('div', { class: 'eng-pkg-name' }, [title]),
					el('div', { class: 'eng-text-sm eng-text-muted' }, [date])
				]),
				msg ? el('div', { class: 'eng-text-sm eng-text-muted eng-mt-1' }, [msg]) : null
			]));
		});
	});
};

/* Register */
pages.register = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Registrasi Kartu'])
	]));
	if (!state.loggedIn) { showAlert(ct, 'Login terlebih dahulu', 'warning'); return; }

	var card = el('div', { class: 'eng-card' });
	card.appendChild(el('h3', { class: 'eng-card-title eng-mb-2' }, ['Registrasi via Dukcapil']));

	var msisdnInput = el('input', { class: 'eng-input', placeholder: 'Nomor MSISDN', type: 'text' });
	var nikInput = el('input', { class: 'eng-input', placeholder: 'NIK (16 digit)', type: 'text' });
	var kkInput = el('input', { class: 'eng-input', placeholder: 'No. KK (16 digit)', type: 'text' });
	var resultDiv = el('div', { class: 'eng-mt-2' });
	var btn = el('button', { class: 'eng-btn eng-btn-primary eng-btn-lg' }, ['Registrasi']);

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
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Bookmark Paket'])
	]));

	var card = el('div', { class: 'eng-card' });
	var body = el('div');
	card.appendChild(body);
	ct.appendChild(card);

	function loadBookmarks() {
		showLoading(body);
		engRpc('get_bookmarks').then(function(r) {
			body.innerHTML = '';
			var bms = (r && r.bookmarks) || [];
			if (bms.length === 0) {
				body.appendChild(el('div', { class: 'eng-text-muted eng-text-center' }, ['Tidak ada bookmark']));
				return;
			}
			var table = el('table', { class: 'eng-table' });
			table.appendChild(el('thead', {}, [el('tr', {}, [
				el('th', {}, ['#']),
				el('th', {}, ['Nama']),
				el('th', {}, ['Option Code']),
				el('th', {}, ['Harga']),
				el('th', {}, ['Aksi'])
			])]));
			var tbody = el('tbody');
			bms.forEach(function(bm, i) {
				var name = bm.name || bm.package_name || '-';
				var oc = bm.option_code || bm.package_option_code || '-';
				var price = bm.price || bm.base_price || 0;
				tbody.appendChild(el('tr', {}, [
					el('td', {}, [String(i + 1)]),
					el('td', {}, [name]),
					el('td', {}, [oc]),
					el('td', {}, [fmtRp(price)]),
					el('td', {}, [
						el('div', { class: 'eng-btn-group' }, [
							el('button', { class: 'eng-btn eng-btn-sm eng-btn-primary', 'data-idx': String(i), onclick: function() {
								showPurchaseModal(bm);
							} }, ['Beli']),
							el('button', { class: 'eng-btn eng-btn-sm eng-btn-danger', 'data-idx': String(i), onclick: function() {
								var idx = parseInt(this.getAttribute('data-idx'));
								if (!confirm('Hapus bookmark?')) return;
								engRpc('delete_bookmark', { index: idx }).then(function() { loadBookmarks(); });
							} }, ['Hapus'])
						])
					])
				]));
			});
			table.appendChild(tbody);
			body.appendChild(table);
		});
	}
	loadBookmarks();
};

/* Settings */
pages.settings = function(ct) {
	ct.appendChild(el('div', { class: 'eng-header' }, [
		el('h1', {}, ['Pengaturan'])
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
		var data = r || {};
		body.appendChild(el('pre', { style: { color: 'var(--eng-text2)', 'font-size': '12px', 'white-space': 'pre-wrap' } }, [JSON.stringify(data, null, 2)]));
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
		var table = el('table', { class: 'eng-table' });
		table.appendChild(el('thead', {}, [el('tr', {}, [
			el('th', {}, ['#']),
			el('th', {}, ['Family Code']),
			el('th', {}, ['Nama'])
		])]));
		var tbody = el('tbody');
		fams.forEach(function(f, i) {
			tbody.appendChild(el('tr', {}, [
				el('td', {}, [String(i + 1)]),
				el('td', {}, [f.code || f.family_code || '-']),
				el('td', {}, [f.name || '-'])
			]));
		});
		table.appendChild(tbody);
		body.appendChild(table);
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
				engRpc('autobuy_control', { action: running ? 'stop' : 'start' }).then(function() {
					navigate('settings');
				});
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

		/* Top header */
		var header = el('div', { class: 'eng-topbar' }, [
			el('span', { class: 'eng-topbar-title' }, ['ENGSEL']),
			el('span', { class: 'eng-topbar-sub' }, ['MyXL Client'])
		]);
		app.appendChild(header);

		/* Horizontal nav bar (glassmorphism iOS style) */
		var navbar = el('div', { class: 'eng-navbar' });
		var navScroll = el('div', { class: 'eng-navbar-scroll' });

		NAV.forEach(function(n) {
			var item = el('div', {
				class: 'eng-nav-pill' + (n.id === state.page ? ' active' : ''),
				onclick: function() { navigate(n.id); }
			}, [
				el('span', { class: 'eng-nav-icon' }, [n.icon]),
				el('span', { class: 'eng-nav-label' }, [n.label])
			]);
			navItems[n.id] = item;
			navScroll.appendChild(item);
		});
		navbar.appendChild(navScroll);
		app.appendChild(navbar);

		/* Main content */
		mainContent = el('div', { class: 'eng-main' });

		app.appendChild(mainContent);
		container.appendChild(app);

		renderPage();

		/* Auto refresh auth if logged in */
		if (state.loggedIn) {
			engRpc('refresh_auth').then(function(r) {
				if (r && r.balance != null) {
					state.balance = r.balance;
					state.expiredAt = r.expired_at || '--';
					var dashNum = document.getElementById('dash-number');
					var dashBal = document.getElementById('dash-balance');
					var dashExp = document.getElementById('dash-exp');
					if (dashNum) dashNum.textContent = state.number;
					if (dashBal) dashBal.textContent = fmtRp(state.balance);
					if (dashExp) dashExp.textContent = 'Aktif sampai: ' + state.expiredAt;
				}
			});
		}

		return container;
	}
});
