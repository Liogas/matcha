#!/bin/sh
set -eu

password_file=/run/secrets/redis_password
if [ ! -r "$password_file" ]; then
    echo "Redis password secret is not readable" >&2
    exit 1
fi

password=$(cat "$password_file")
if [ -z "$password" ]; then
    echo "Redis password secret is empty" >&2
    exit 1
fi

password_hash=$(printf '%s' "$password" | sha256sum | cut -d ' ' -f 1)
acl_file=/tmp/redis-users.acl
printf 'user default on #%s ~matcha:login-attempt:* +@connection +ping +eval +evalsha +incr +expire +ttl +del\n' "$password_hash" > "$acl_file"
chmod 600 "$acl_file"
unset password password_hash

exec redis-server --aclfile "$acl_file" --save "" --appendonly no
