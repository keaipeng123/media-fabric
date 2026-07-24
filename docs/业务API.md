# 业务 API：认证与用户管理

Go 宿主在启用 `[api] enabled = true` 后提供 HTTP API。默认仅监听 `127.0.0.1:8080`，建议由反向代理提供 TLS。

## 首次配置

```ini
[api]
enabled = true
listen_address = 127.0.0.1:8080
database_url = postgres://media_fabric:strong-password@127.0.0.1:5432/media_fabric?sslmode=disable
jwt_secret = replace-with-a-random-secret-at-least-32-characters
bootstrap_admin_username = admin
bootstrap_admin_password = replace-with-a-strong-initial-password
```

也可使用环境变量覆盖敏感配置：`MEDIA_FABRIC_DATABASE_URL`、`MEDIA_FABRIC_JWT_SECRET`、`MEDIA_FABRIC_BOOTSTRAP_ADMIN_PASSWORD`。

服务首次启动时会创建管理员；已有用户时不会覆盖管理员密码。

## API

- `GET /api/v1/health`
- `POST /api/v1/auth/login`
- `POST /api/v1/auth/refresh`
- `POST /api/v1/auth/logout`
- `GET /api/v1/me`
- `GET /api/v1/users`（管理员）
- `POST /api/v1/users`（管理员）
- `PATCH /api/v1/users/{id}`（管理员）

登录请求：

```json
{"username":"admin","password":"your-password"}
```

成功响应包含 `access_token` 和 `refresh_token`。受保护接口使用：

```text
Authorization: Bearer <access_token>
```
