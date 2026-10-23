'use strict';
// HTTP 오류 봉투 — 모든 로컬 도구가 같은 모양으로 실패를 말한다.
//
//   { error: { code, message, details } }
//
// code 는 안정적이어야 UI 가 상황별 안내(재시도/에디터 켜기/복구 절차)를 고를 수 있다.
// 앱마다 고유 code 가 있으므로 상태 표는 BASE 를 깔고 앱이 덮어쓰는 구조다.

class ApiError extends Error {
  constructor(code, message, details = undefined) {
    super(message);
    this.name = 'ApiError';
    this.code = code;
    if (details !== undefined) this.details = details;
  }
}

// 어느 도구에나 있는 공통 실패. 앱 고유 code 는 앱이 더한다.
const BASE_STATUS_BY_CODE = Object.freeze({
  UNAUTHORIZED: 401,
  BAD_REQUEST: 400, BAD_JSON: 400, BAD_FIELD: 400,
  NOT_FOUND: 404, JOB_NOT_FOUND: 404,
  QUEUE_BUSY: 409,
  BODY_TOO_LARGE: 413,
  UNSUPPORTED_MEDIA_TYPE: 415,
  QUEUE_CLOSED: 503, SHUTTING_DOWN: 503,
  NOT_IMPLEMENTED: 501,
});

const errorBody = e => ({
  error: {
    code: (e && e.code) || 'INTERNAL',
    message: (e && e.message) || String(e),
    details: (e && e.details) || {},
  },
});

// 표에 없는 code 는 500 이다 — 모르는 실패를 200 으로 흘리지 않는다.
const makeStatusOf = table => e => table[(e && e.code) || ''] || 500;

module.exports = { ApiError, BASE_STATUS_BY_CODE, errorBody, makeStatusOf };
