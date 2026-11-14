'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

async function main() {
  const html = fs.readFileSync(
    path.join(__dirname, '..', 'tools', 'server-protection', 'dashboard', 'index.html'),
    'utf8'
  );
  const server = http.createServer((request, response) => {
    if (request.url === '/' || request.url === '/index.html') {
      response.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      response.end(html);
      return;
    }
    response.writeHead(404);
    response.end();
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const address = server.address();
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();

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

  await page.route('**/api/**', async route => {
    const request = route.request();
    const url = new URL(request.url());
    const method = request.method();
    calls.push({ path: url.pathname, method, headers: request.headers() });

    if (url.pathname === '/api/status') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          blocked: 12, score: 140, score_label: 'HIGH',
          bitwire_estimate: 3, ipsum_estimate: 4, root: true
        })
      });
      return;
    }
    if (url.pathname === '/api/bans') {
      await route.fulfill({
        status: 200, contentType: 'application/json',
        body: JSON.stringify({ bans })
      });
      return;
    }
    if (url.pathname === '/api/whitelist' && method === 'GET') {
      await route.fulfill({
        status: 200, contentType: 'application/json',
        body: JSON.stringify({ whitelist })
      });
      return;
    }
    if (url.pathname === '/api/unblock' && method === 'POST') {
      if (failUnblock) {
        await route.fulfill({
          status: 503, contentType: 'application/json',
          body: JSON.stringify({ error: 'API unavailable' })
        });
        return;
      }
      const payload = request.postDataJSON();
      bans = bans.filter(item => item.ip !== payload.ip);
      whitelist = whitelist.concat([{ entry: payload.ip, type: 'ip' }]);
      await route.fulfill({
        status: 200, contentType: 'application/json',
        body: JSON.stringify({ ok: true })
      });
      return;
    }
    await route.fulfill({ status: 404, body: 'not found' });
  });

  try {
    await page.goto('http://127.0.0.1:' + address.port + '/');
    await page.waitForFunction(() =>
      document.getElementById('scoreNum') &&
      document.getElementById('scoreNum').textContent === '12'
    );

    assert.equal(await page.locator('#scoreNum').textContent(), '12');
    assert.equal(await page.locator('#scoreSt').textContent(), 'CSF DENY ENTRIES');
    assert.equal(await page.locator('#banList img').count(), 0,
      'untrusted ban metadata is rendered as text, not HTML');
    assert.match(await page.locator('#banList').textContent(), /<img src=x onerror=alert\(1\)>/);

    await page.evaluate(async () => {
      selectBan(0);
      await doUnblock();
    });
    await page.waitForFunction(() =>
      document.getElementById('banList').textContent.indexOf('203.0.113.10') < 0 &&
      document.getElementById('wlList').textContent.indexOf('203.0.113.10') >= 0
    );

    const unblockCall = calls.find(call => call.path === '/api/unblock');
    assert.ok(unblockCall, 'unblock request reached the API');
    assert.equal(unblockCall.method, 'POST');
    assert.match(unblockCall.headers['content-type'], /application\/json/i);

    bans = [{
      ip: '203.0.113.11', cc: 'ZZ', org: 'API failure test',
      set: 'TEST', hits: 1, active: true
    }];
    await page.evaluate(() => refreshFromApi());
    await page.waitForFunction(() =>
      document.getElementById('banList').textContent.indexOf('203.0.113.11') >= 0
    );
    failUnblock = true;
    await page.evaluate(async () => {
      selectBan(0);
      await doUnblock();
    });
    assert.match(await page.locator('#banList').textContent(), /203\.0\.113\.11/,
      'failed API mutation does not locally remove the ban');
    assert.doesNotMatch(await page.locator('#wlList').textContent(), /203\.0\.113\.11/,
      'failed API mutation does not locally add a whitelist entry');

    console.log('Dashboard browser integration tests passed.');
  } finally {
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
