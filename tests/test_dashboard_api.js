'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const htmlPath = path.join(__dirname, '..', 'tools', 'server-protection', 'dashboard', 'index.html');
const html = fs.readFileSync(htmlPath, 'utf8');
const scripts = Array.from(html.matchAll(/<script>([\s\S]*?)<\/script>/g), match => match[1]);
assert.equal(scripts.length, 2, 'dashboard scripts remain separate and ordered');

const elements = new Map();
function getElementById(id) {
  if (!elements.has(id)) {
    elements.set(id, {
      id,
      innerHTML: '',
      textContent: '',
      value: '',
      disabled: false,
      style: {},
      classList: { add() {}, remove() {} }
    });
  }
  return elements.get(id);
}

let bans = [{
  ip: '203.0.113.10',
  cc: 'ZZ',
  org: '<img src=x onerror=alert(1)>',
  set: 'TEST',
  hits: 1,
  active: true
}];
let whitelist = [];
let failUnblock = false;
const calls = [];

async function fetchMock(url, init) {
  init = init || {};
  const pathname = new URL(url, 'http://localhost').pathname;
  const method = init.method || 'GET';
  calls.push({ pathname, method, headers: init.headers || {}, body: init.body || null });

  if (pathname === '/api/status') {
    return { ok: true, status: 200, json: async () => ({
      blocked: 12,
      score: 140,
      score_label: 'HIGH',
      bitwire_estimate: 3,
      ipsum_estimate: 4,
      root: true
    }) };
  }
  if (pathname === '/api/bans') {
    return { ok: true, status: 200, json: async () => ({ bans }) };
  }
  if (pathname === '/api/whitelist' && method === 'GET') {
    return { ok: true, status: 200, json: async () => ({ whitelist }) };
  }
  if (pathname === '/api/unblock' && method === 'POST') {
    if (failUnblock) {
      return { ok: false, status: 503, json: async () => ({ error: 'API unavailable' }) };
    }
    const payload = JSON.parse(init.body);
    bans = bans.filter(item => item.ip !== payload.ip);
    whitelist = whitelist.concat([{ entry: payload.ip, type: 'ip' }]);
    return { ok: true, status: 200, json: async () => ({ ok: true }) };
  }
  throw new Error('Unexpected API request: ' + method + ' ' + pathname);
}

const context = {
  document: {
    getElementById,
    querySelectorAll: () => []
  },
  location: { protocol: 'http:' },
  fetch: fetchMock,
  setInterval: () => 1,
  clearInterval: () => {},
  setTimeout: () => 1,
  console,
  Math,
  Date,
  Number,
  String,
  Array,
  JSON,
  Promise,
  URL
};
vm.createContext(context);
for (const source of scripts) vm.runInContext(source, context, { filename: 'dashboard-inline.js' });

async function flush() {
  await new Promise(resolve => setImmediate(resolve));
}

(async () => {
  await flush();
  assert.equal(getElementById('scoreNum').textContent, '12', 'dashboard displays the authoritative CSF deny count');
  assert.equal(getElementById('scoreSt').textContent, 'CSF DENY ENTRIES',
    'dashboard does not turn firewall counts into a threat score');
  assert.equal(getElementById('apiStatusValue').textContent, 'LIVE');
  assert.match(getElementById('banList').innerHTML, /&lt;img/);
  assert.doesNotMatch(getElementById('banList').innerHTML, /<img/);

  context.selectBan(0);
  await context.doUnblock();
  await flush();

  const unblock = calls.find(call => call.pathname === '/api/unblock');
  assert.ok(unblock, 'unblock uses the API');
  assert.equal(unblock.method, 'POST');
  assert.match(unblock.headers['Content-Type'], /application\/json/i);
  assert.deepEqual(JSON.parse(unblock.body), { ip: '203.0.113.10' });
  assert.equal(getElementById('banList').innerHTML, '', 'ban list refreshes after unblock');
  assert.match(getElementById('wlList').innerHTML, /203\.0\.113\.10/, 'whitelist refreshes after unblock');

  bans = [{
    ip: '203.0.113.11', cc: 'ZZ', org: 'API failure test',
    set: 'TEST', hits: 1, active: true
  }];
  await context.refreshFromApi();
  context.selectBan(0);
  failUnblock = true;
  await context.doUnblock();
  assert.match(getElementById('banList').innerHTML, /203\.0\.113\.11/,
    'failed API mutation does not locally remove the ban');
  assert.equal(whitelist.length, 1,
    'failed API mutation does not locally add a whitelist entry');

  console.log('Dashboard API integration tests passed.');
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
