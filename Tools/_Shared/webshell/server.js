'use strict';
// 127.0.0.1 전용 HTTP 셸 — 정적 서빙, 접속 키, 종료. (Balance Studio 에서 추출)
//
// 보안 모델(로컬 도구지만 브라우저는 아무 페이지나 띄울 수 있다):
//  · 서버는 loopback 에만 bind 한다. 외부에서 접속할 수 없다.
//  · 실행마다 32바이트 랜덤 키를 만든다. 첫 진입 URL(?key=)로만 세션 쿠키를 준다.
//  · 정적 파일은 쿠키, API 는 커스텀 토큰 헤더를 요구한다.
//    → 쿠키는 자동 전송되지만 커스텀 헤더는 cross-origin 으로 못 붙는다 = CSRF 차단.
//  · 파일 경로를 요청에서 받지 않는다. 고정 목록에 있는 이름만 연다.
//
// 앱마다 갈라지는 건 4가지뿐이다: appName · publicDir/staticFiles · cookieName · tokenHeader.
// 라우터와 종료 시 정리(onDrain)는 앱이 주입한다 — 셸은 앱의 도메인을 모른다.
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');

const { ApiError, errorBody, makeStatusOf, BASE_STATUS_BY_CODE } = require('./api-error.js');

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
};
const MAX_BODY_BYTES = 65_536;

const timingSafeEqual = (a, b) => {
  const ab = Buffer.from(String(a));
  const bb = Buffer.from(String(b));
  return ab.length === bb.length && crypto.timingSafeEqual(ab, bb);
};

function parseCookies(header) {
  const out = {};
  for (const part of String(header || '').split(';')) {
    const at = part.indexOf('=');
    if (at < 0) continue;
    out[part.slice(0, at).trim()] = part.slice(at + 1).trim();
  }
  return out;
}

function openInBrowser(url) {
  try {
    // shell 문자열을 만들지 않는다 — URL 에 & 가 있어도 안전하다.
    spawn('cmd', ['/c', 'start', '', url], { detached: true, stdio: 'ignore', windowsHide: true }).unref();
  } catch { /* 브라우저를 못 열어도 콘솔의 주소로 열면 된다 */ }
}

function createWebShellServer({
  host = '127.0.0.1',
  port = 0,
  token,
  appName = 'Local Tool',
  publicDir,
  staticFiles = [],
  cookieName,
  tokenHeader,
  createRouter,
  onDrain = null,
  statusOf = makeStatusOf(BASE_STATUS_BY_CODE),
  maxBodyBytes = MAX_BODY_BYTES,
  openBrowser = false,
  log = () => {},
}) {
  if (!token) throw new Error('createWebShellServer: token 이 필요합니다.');
  if (!publicDir) throw new Error('createWebShellServer: publicDir 이 필요합니다.');
  if (!cookieName) throw new Error('createWebShellServer: cookieName 이 필요합니다.');
  if (!tokenHeader) throw new Error('createWebShellServer: tokenHeader 가 필요합니다.');
  if (typeof createRouter !== 'function') throw new Error('createWebShellServer: createRouter 가 필요합니다.');

  let shuttingDown = false;
  let closeSelf = null; // start() 이후 채워진다 — shutdown 응답을 보낸 뒤에 닫아야 한다
  const router = createRouter({
    isShuttingDown: () => shuttingDown,
    requestShutdown: () => {
      shuttingDown = true;
      // 응답을 먼저 흘려보내고 닫는다 — 지금 닫으면 클라이언트는 결과 대신 연결 오류를 본다.
      setTimeout(() => { if (closeSelf) closeSelf(); }, 50).unref();
    },
  });
  const sockets = new Set();

  const send = (res, status, body, extraHeaders = {}) => {
    const text = JSON.stringify(body);
    res.writeHead(status, {
      'Content-Type': 'application/json; charset=utf-8',
      'Content-Length': Buffer.byteLength(text),
      'Cache-Control': 'no-store',
      'X-Content-Type-Options': 'nosniff',
      ...extraHeaders,
    });
    res.end(text);
  };

  function readBody(req) {
    return new Promise((resolve, reject) => {
      const type = String(req.headers['content-type'] || '').split(';')[0].trim();
      if (type !== 'application/json') {
        return reject(new ApiError('UNSUPPORTED_MEDIA_TYPE', 'Content-Type 은 application/json 이어야 합니다.'));
      }
      let size = 0;
      let rejected = false;
      const chunks = [];
      req.on('data', chunk => {
        if (rejected) return;
        size += chunk.length;
        if (size > maxBodyBytes) {
          rejected = true;
          chunks.length = 0;
          // ⚠ 여기서 req.destroy() 하면 413 을 쓰기도 전에 연결이 끊겨 클라이언트는 "fetch failed" 만 본다.
          //   남은 본문은 버리며 흘려보내고, 응답은 정상적으로 413 으로 내보낸다.
          req.resume();
          reject(new ApiError('BODY_TOO_LARGE', `요청 본문이 너무 큽니다 (최대 ${maxBodyBytes} bytes).`));
          return;
        }
        chunks.push(chunk);
      });
      req.on('end', () => {
        if (rejected) return;
        if (!chunks.length) return resolve({});
        try {
          resolve(JSON.parse(Buffer.concat(chunks).toString('utf8')));
        } catch (e) {
          reject(new ApiError('BAD_JSON', `본문 JSON 을 읽지 못했습니다: ${e.message}`));
        }
      });
      req.on('error', reject);
    });
  }

  function serveStatic(res, name) {
    const file = path.join(publicDir, name);
    let text;
    try {
      text = fs.readFileSync(file);
    } catch {
      return send(res, 404, errorBody(new ApiError('NOT_FOUND', `파일이 없습니다: ${name}`)));
    }
    res.writeHead(200, {
      'Content-Type': MIME[path.extname(name)] || 'application/octet-stream',
      'Content-Length': text.length,
      'Cache-Control': 'no-store',
      'X-Content-Type-Options': 'nosniff',
    });
    res.end(text);
  }

  const server = http.createServer(async (req, res) => {
    let url;
    try {
      url = new URL(req.url, `http://${host}`);
    } catch {
      return send(res, 400, errorBody(new ApiError('BAD_REQUEST', '요청 경로를 해석하지 못했습니다.')));
    }
    const pathname = url.pathname;

    // 경로 트래버설 차단 — 고정 목록을 쓰지만 인코딩된 구분자/널바이트는 먼저 세운다.
    if (pathname.includes('..') || pathname.includes('\0') || /%2f/i.test(req.url) || /%5c/i.test(req.url)) {
      return send(res, 400, errorBody(new ApiError('BAD_REQUEST', '허용되지 않은 경로입니다.')));
    }

    if (pathname.startsWith('/api/')) {
      if (!timingSafeEqual(req.headers[tokenHeader] || '', token)) {
        return send(res, 401, errorBody(new ApiError('UNAUTHORIZED', '접속 키가 없거나 올바르지 않습니다. 도구를 다시 실행해 새 주소로 여세요.')));
      }
      if (req.method !== 'GET' && req.method !== 'POST') {
        return send(res, 405, errorBody(new ApiError('BAD_REQUEST', `허용되지 않은 메서드: ${req.method}`)));
      }
      try {
        const body = req.method === 'POST' ? await readBody(req) : {};
        const out = await router.handle({ method: req.method, pathname, query: url.searchParams, body, req });
        return send(res, out.status, out.body);
      } catch (e) {
        return send(res, statusOf(e), errorBody(e));
      }
    }

    if (req.method !== 'GET') {
      return send(res, 405, errorBody(new ApiError('BAD_REQUEST', `허용되지 않은 메서드: ${req.method}`)));
    }

    const isEntry = pathname === '/' || pathname === '/index.html';
    const cookieOk = timingSafeEqual(parseCookies(req.headers.cookie)[cookieName] || '', token);
    const keyOk = timingSafeEqual(url.searchParams.get('key') || '', token);

    if (isEntry && keyOk && !cookieOk) {
      // 첫 진입 — 키를 세션 쿠키로 옮긴다. 이후 요청의 URL 에는 키가 남지 않는다.
      res.setHeader('Set-Cookie', `${cookieName}=${token}; HttpOnly; SameSite=Strict; Path=/`);
    } else if (!cookieOk && !(isEntry && keyOk)) {
      return send(res, 401, errorBody(new ApiError('UNAUTHORIZED', '접속 키가 없습니다 — 도구가 출력한 주소로 여세요.')));
    }

    const name = isEntry ? 'index.html' : staticFiles.find(f => `/${f}` === pathname);
    if (!name) return send(res, 404, errorBody(new ApiError('NOT_FOUND', `없는 경로입니다: ${pathname}`)));
    return serveStatic(res, name);
  });

  server.on('connection', socket => {
    sockets.add(socket);
    socket.on('close', () => sockets.delete(socket));
  });

  let address = null;
  const shell = {
    router,
    get address() { return address; },
    start() {
      return new Promise((resolve, reject) => {
        server.once('error', reject);
        server.listen(port, host, () => {
          const a = server.address();
          address = { host, port: a.port, url: `http://${host}:${a.port}` };
          if (openBrowser) openInBrowser(`${address.url}/?key=${token}`);
          log(`${appName}: ${address.url}/?key=${token}`);
          resolve(address);
        });
      });
    },
    async close({ drainMs = 5000 } = {}) {
      shuttingDown = true;
      // 실행 중인 작업에 정리할 시간을 주되, 영원히 기다리지는 않는다.
      if (onDrain) {
        try {
          await Promise.race([
            onDrain({ drainMs }),
            new Promise(resolve => setTimeout(resolve, drainMs).unref()),
          ]);
        } catch { /* 종료 중 실패는 삼킨다 */ }
      }
      await new Promise(resolve => {
        server.close(resolve);
        for (const s of sockets) s.destroy(); // keep-alive 소켓이 남으면 close 가 안 끝난다
      });
    },
    get shuttingDown() { return shuttingDown; },
  };
  closeSelf = () => shell.close();
  return shell;
}

module.exports = { createWebShellServer, openInBrowser, parseCookies, timingSafeEqual, MIME, MAX_BODY_BYTES };
