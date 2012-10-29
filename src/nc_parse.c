/*
 * twemproxy - A fast and lightweight proxy for memcached protocol.
 * Copyright (C) 2011 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>

#include <nc_core.h>
#include <nc_parse.h>

/*
 * Return true, if the redis command accepts no arguments, otherwise
 * return false
 */
static bool
parse_arg0(struct msg *r)
{
    switch (r->type) {
    case MSG_REQ_REDIS_EXISTS:
    case MSG_REQ_REDIS_PERSIST:
    case MSG_REQ_REDIS_PTTL:
    case MSG_REQ_REDIS_TTL:
    case MSG_REQ_REDIS_TYPE:

    case MSG_REQ_REDIS_DECR:
    case MSG_REQ_REDIS_GET:
    case MSG_REQ_REDIS_INCR:
    case MSG_REQ_REDIS_STRLEN:

    case MSG_REQ_REDIS_HGETALL:
    case MSG_REQ_REDIS_HKEYS:
    case MSG_REQ_REDIS_HLEN:
    case MSG_REQ_REDIS_HVALS:

    case MSG_REQ_REDIS_LLEN:
    case MSG_REQ_REDIS_LPOP:
    case MSG_REQ_REDIS_RPOP:

    case MSG_REQ_REDIS_SCARD:
    case MSG_REQ_REDIS_SMEMBERS:
    case MSG_REQ_REDIS_SPOP:
    case MSG_REQ_REDIS_SRANDMEMBER:

    case MSG_REQ_REDIS_ZCARD:
        return true;

    default:
        break;
    }

    return false;
}

/*
 * Return true, if the redis command accepts exactly 1 argument, otherwise
 * return false
 */
static bool
parse_arg1(struct msg *r)
{
    switch (r->type) {
    case MSG_REQ_REDIS_EXPIRE:
    case MSG_REQ_REDIS_EXPIREAT:
    case MSG_REQ_REDIS_PEXPIRE:
    case MSG_REQ_REDIS_PEXPIREAT:

    case MSG_REQ_REDIS_APPEND:
    case MSG_REQ_REDIS_DECRBY:
    case MSG_REQ_REDIS_GETBIT:
    case MSG_REQ_REDIS_GETSET:
    case MSG_REQ_REDIS_INCRBY:
    case MSG_REQ_REDIS_INCRBYFLOAT:
    case MSG_REQ_REDIS_SET:
    case MSG_REQ_REDIS_SETNX:

    case MSG_REQ_REDIS_HEXISTS:
    case MSG_REQ_REDIS_HGET:

    case MSG_REQ_REDIS_LINDEX:
    case MSG_REQ_REDIS_LPUSHX:
    case MSG_REQ_REDIS_RPUSHX:

    case MSG_REQ_REDIS_SISMEMBER:

    case MSG_REQ_REDIS_ZRANK:
    case MSG_REQ_REDIS_ZREVRANK:
    case MSG_REQ_REDIS_ZSCORE:
        return true;

    default:
        break;
    }

    return false;
}

/*
 * Return true, if the redis command accepts exactly 2 arguments, otherwise
 * return false
 */
static bool
parse_arg2(struct msg *r)
{
    switch (r->type) {
    case MSG_REQ_REDIS_GETRANGE:
    case MSG_REQ_REDIS_PSETEX:
    case MSG_REQ_REDIS_SETBIT:
    case MSG_REQ_REDIS_SETEX:
    case MSG_REQ_REDIS_SETRANGE:

    case MSG_REQ_REDIS_HINCRBY:
    case MSG_REQ_REDIS_HINCRBYFLOAT:
    case MSG_REQ_REDIS_HSET:
    case MSG_REQ_REDIS_HSETNX:

    case MSG_REQ_REDIS_LRANGE:
    case MSG_REQ_REDIS_LREM:
    case MSG_REQ_REDIS_LSET:
    case MSG_REQ_REDIS_LTRIM:

    case MSG_REQ_REDIS_ZCOUNT:
    case MSG_REQ_REDIS_ZINCRBY:
    case MSG_REQ_REDIS_ZREMRANGEBYRANK:
    case MSG_REQ_REDIS_ZREMRANGEBYSCORE:
        return true;

    default:
        break;
    }

    return false;
}

/*
 * Return true, if the redis command accepts exactly 3 arguments, otherwise
 * return false
 */
static bool
parse_arg3(struct msg *r)
{
    switch (r->type) {
    case MSG_REQ_REDIS_LINSERT:
        return true;

    default:
        break;
    }
    return false;
}

/*
 * Return true, if the redis command accepts 0 or more arguments, otherwise
 * return false
 */
static bool
parse_argn(struct msg *r)
{
    switch (r->type) {
    case MSG_REQ_REDIS_BITCOUNT:

    case MSG_REQ_REDIS_HDEL:
    case MSG_REQ_REDIS_HMGET:
    case MSG_REQ_REDIS_HMSET:

    case MSG_REQ_REDIS_LPUSH:
    case MSG_REQ_REDIS_RPUSH:

    case MSG_REQ_REDIS_SADD:
    case MSG_REQ_REDIS_SREM:

    case MSG_REQ_REDIS_ZADD:
    case MSG_REQ_REDIS_ZRANGE:
    case MSG_REQ_REDIS_ZRANGEBYSCORE:
    case MSG_REQ_REDIS_ZREM:
    case MSG_REQ_REDIS_ZREVRANGE:
    case MSG_REQ_REDIS_ZREVRANGEBYSCORE:
        return true;

    default:
        break;
    }

    return false;
}

static bool
parse_argx(struct msg *r)
{
    switch (r->type) {
    case MSG_REQ_REDIS_MGET:
    case MSG_REQ_REDIS_DEL:
        return true;

    default:
        break;
    }

    return false;
}

/*
 * Reference: http://redis.io/topics/protocol
 *
 * Redis >= 1.2 uses the unified protocol to send requests to the Redis
 * server. In the the unified protocol all the arguments sent to the server
 * are binary safe and every request has the following general form:
 *
 *   *<number of arguments> CR LF
 *   $<number of bytes of argument 1> CR LF
 *   <argument data> CR LF
 *   ...
 *   $<number of bytes of argument N> CR LF
 *   <argument data> CR LF
 *
 * Before the unified request protocol, redis protocol for requests supported
 * the following commands
 * 1). Inline commands: simple commands where arguments are just space
 *     separated strings. No binary safeness is possible.
 * 2). Bulk commands: bulk commands are exactly like inline commands, but
 *     the last argument is handled in a special way in order to allow for
 *     a binary-safe last argument.
 *
 * Nutcracker only supports the Redis unified protocol for requests.
 */
void
parse_request(struct msg *r)
{
    struct mbuf *b;
    uint8_t *p, *m;
    uint8_t ch;
    enum {
        SW_START,
        SW_NARG,
        SW_NARG_LF,
        SW_REQ_TYPE_LEN,
        SW_REQ_TYPE_LEN_LF,
        SW_REQ_TYPE,
        SW_REQ_TYPE_LF,
        SW_KEY_LEN,
        SW_KEY_LEN_LF,
        SW_KEY,
        SW_KEY_LF,
        SW_ARG1_LEN,
        SW_ARG1_LEN_LF,
        SW_ARG1,
        SW_ARG1_LF,
        SW_ARG2_LEN,
        SW_ARG2_LEN_LF,
        SW_ARG2,
        SW_ARG2_LF,
        SW_ARG3_LEN,
        SW_ARG3_LEN_LF,
        SW_ARG3,
        SW_ARG3_LF,
        SW_ARGN_LEN,
        SW_ARGN_LEN_LF,
        SW_ARGN,
        SW_ARGN_LF,
        SW_FRAGMENT,
        SW_SENTINEL
    } state;

    state = r->state;
    b = STAILQ_LAST(&r->mhdr, mbuf, next);

    ASSERT(r->request);
    ASSERT(state >= SW_START && state < SW_SENTINEL);
    ASSERT(b != NULL);
    ASSERT(b->pos <= b->last);

    /* validate the parsing maker */
    ASSERT(r->pos != NULL);
    ASSERT(r->pos >= b->pos && r->pos <= b->last);

    for (p = r->pos; p < b->last; p++) {
        ch = *p;

        switch (state) {

        case SW_START:
        case SW_NARG:
            if (r->token == NULL) {
                if (ch != '*') {
                    goto error;
                }
                r->token = p;
                /* req_start <- p */
                r->narg_start = p;
                r->rnarg = 0;
                state = SW_NARG;
            } else if (isdigit(ch)) {
                r->rnarg = r->rnarg * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if (r->rnarg == 0) {
                    goto error;
                }
                r->narg = r->rnarg;
                r->narg_end = p;
                r->token = NULL;
                state = SW_NARG_LF;
            } else {
                goto error;
            }

            break;

        case SW_NARG_LF:
            switch (ch) {
            case LF:
                state = SW_REQ_TYPE_LEN;
                break;

            default:
                goto error;
            }

            break;

        case SW_REQ_TYPE_LEN:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                r->token = p;
                r->rlen = 0;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if (r->rlen == 0 || r->rnarg == 0) {
                    goto error;
                }
                r->rnarg--;
                r->token = NULL;
                state = SW_REQ_TYPE_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_REQ_TYPE_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_REQ_TYPE;
                break;

            default:
                goto error;
            }

            break;

        case SW_REQ_TYPE:
            if (r->token == NULL) {
                r->token = p;
            }

            m = p + r->rlen;
            if (m >= b->last) {
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            r->rlen = 0;
            m = r->token;
            r->token = NULL;
            r->type = MSG_UNKNOWN;

            switch (p - m) {

            case 3:
                if (str3icmp(m, 'g', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_GET;
                    break;
                }

                if (str3icmp(m, 's', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_SET;
                    break;
                }

                if (str3icmp(m, 't', 't', 'l')) {
                    r->type = MSG_REQ_REDIS_TTL;
                    break;
                }

                if (str3icmp(m, 'd', 'e', 'l')) {
                    r->type = MSG_REQ_REDIS_DEL;
                    break;
                }

                break;

            case 4:
                if (str4icmp(m, 'p', 't', 't', 'l')) {
                    r->type = MSG_REQ_REDIS_PTTL;
                    break;
                }

                if (str4icmp(m, 'd', 'e', 'c', 'r')) {
                    r->type = MSG_REQ_REDIS_DECR;
                    break;
                }

                if (str4icmp(m, 'h', 'd', 'e', 'l')) {
                    r->type = MSG_REQ_REDIS_HDEL;
                    break;
                }

                if (str4icmp(m, 'h', 'g', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_HGET;
                    break;
                }

                if (str4icmp(m, 'h', 'l', 'e', 'n')) {
                    r->type = MSG_REQ_REDIS_HLEN;
                    break;
                }

                if (str4icmp(m, 'h', 's', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_HSET;
                    break;
                }

                if (str4icmp(m, 'i', 'n', 'c', 'r')) {
                    r->type = MSG_REQ_REDIS_INCR;
                    break;
                }

                if (str4icmp(m, 'l', 'l', 'e', 'n')) {
                    r->type = MSG_REQ_REDIS_LLEN;
                    break;
                }

                if (str4icmp(m, 'l', 'p', 'o', 'p')) {
                    r->type = MSG_REQ_REDIS_LPOP;
                    break;
                }

                if (str4icmp(m, 'l', 'r', 'e', 'm')) {
                    r->type = MSG_REQ_REDIS_LREM;
                    break;
                }

                if (str4icmp(m, 'l', 's', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_LSET;
                    break;
                }

                if (str4icmp(m, 'r', 'p', 'o', 'p')) {
                    r->type = MSG_REQ_REDIS_RPOP;
                    break;
                }

                if (str4icmp(m, 's', 'a', 'd', 'd')) {
                    r->type = MSG_REQ_REDIS_SADD;
                    break;
                }

                if (str4icmp(m, 's', 'p', 'o', 'p')) {
                    r->type = MSG_REQ_REDIS_SPOP;
                    break;
                }

                if (str4icmp(m, 's', 'r', 'e', 'm')) {
                    r->type = MSG_REQ_REDIS_SREM;
                    break;
                }

                if (str4icmp(m, 't', 'y', 'p', 'e')) {
                    r->type = MSG_REQ_REDIS_TYPE;
                    break;
                }

                if (str4icmp(m, 'm', 'g', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_MGET;
                    break;
                }

                if (str4icmp(m, 'z', 'a', 'd', 'd')) {
                    r->type = MSG_REQ_REDIS_ZADD;
                    break;
                }

                if (str4icmp(m, 'z', 'r', 'e', 'm')) {
                    r->type = MSG_REQ_REDIS_ZREM;
                    break;
                }

                break;

            case 5:
                if (str5icmp(m, 'h', 'k', 'e', 'y', 's')) {
                    r->type = MSG_REQ_REDIS_HKEYS;
                    break;
                }

                if (str5icmp(m, 'h', 'm', 'g', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_HMGET;
                    break;
                }

                if (str5icmp(m, 'h', 'm', 's', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_HMSET;
                    break;
                }

                if (str5icmp(m, 'h', 'v', 'a', 'l', 's')) {
                    r->type = MSG_REQ_REDIS_HVALS;
                    break;
                }

                if (str5icmp(m, 'l', 'p', 'u', 's', 'h')) {
                    r->type = MSG_REQ_REDIS_LPUSH;
                    break;
                }

                if (str5icmp(m, 'l', 't', 'r', 'i', 'm')) {
                    r->type = MSG_REQ_REDIS_LTRIM;
                    break;
                }

                if (str5icmp(m, 'r', 'p', 'u', 's', 'h')) {
                    r->type = MSG_REQ_REDIS_RPUSH;
                    break;
                }

                if (str5icmp(m, 's', 'c', 'a', 'r', 'd')) {
                    r->type = MSG_REQ_REDIS_SCARD;
                    break;
                }

                if (str5icmp(m, 's', 'e', 't', 'e', 'x')) {
                    r->type = MSG_REQ_REDIS_SETEX;
                    break;
                }

                if (str5icmp(m, 's', 'e', 't', 'n', 'x')) {
                    r->type = MSG_REQ_REDIS_SETNX;
                    break;
                }

                if (str5icmp(m, 'z', 'c', 'a', 'r', 'd')) {
                    r->type = MSG_REQ_REDIS_ZCARD;
                    break;
                }

                if (str5icmp(m, 'z', 'r', 'a', 'n', 'k')) {
                    r->type = MSG_REQ_REDIS_ZRANK;
                    break;
                }

                break;

            case 6:
                if (str6icmp(m, 'a', 'p', 'p', 'e', 'n', 'd')) {
                    r->type = MSG_REQ_REDIS_APPEND;
                    break;
                }

                if (str6icmp(m, 'd', 'e', 'c', 'r', 'b', 'y')) {
                    r->type = MSG_REQ_REDIS_DECRBY;
                    break;
                }

                if (str6icmp(m, 'e', 'x', 'i', 's', 't', 's')) {
                    r->type = MSG_REQ_REDIS_EXISTS;
                    break;
                }

                if (str6icmp(m, 'e', 'x', 'p', 'i', 'r', 'e')) {
                    r->type = MSG_REQ_REDIS_EXPIRE;
                    break;
                }

                if (str6icmp(m, 'g', 'e', 't', 'b', 'i', 't')) {
                    r->type = MSG_REQ_REDIS_GETBIT;
                    break;
                }

                if (str6icmp(m, 'g', 'e', 't', 's', 'e', 't')) {
                    r->type = MSG_REQ_REDIS_GETSET;
                    break;
                }

                if (str6icmp(m, 'p', 's', 'e', 't', 'e', 'x')) {
                    r->type = MSG_REQ_REDIS_PSETEX;
                    break;
                }

                if (str6icmp(m, 'h', 's', 'e', 't', 'n', 'x')) {
                    r->type = MSG_REQ_REDIS_HSETNX;
                    break;
                }

                if (str6icmp(m, 'i', 'n', 'c', 'r', 'b', 'y')) {
                    r->type = MSG_REQ_REDIS_INCRBY;
                    break;
                }

                if (str6icmp(m, 'l', 'i', 'n', 'd', 'e', 'x')) {
                    r->type = MSG_REQ_REDIS_LINDEX;
                    break;
                }

                if (str6icmp(m, 'l', 'p', 'u', 's', 'h', 'x')) {
                    r->type = MSG_REQ_REDIS_LPUSHX;
                    break;
                }

                if (str6icmp(m, 'l', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = MSG_REQ_REDIS_LRANGE;
                    break;
                }

                if (str6icmp(m, 'r', 'p', 'u', 's', 'h', 'x')) {
                    r->type = MSG_REQ_REDIS_RPUSHX;
                    break;
                }

                if (str6icmp(m, 's', 'e', 't', 'b', 'i', 't')) {
                    r->type = MSG_REQ_REDIS_SETBIT;
                    break;
                }

                if (str6icmp(m, 's', 't', 'r', 'l', 'e', 'n')) {
                    r->type = MSG_REQ_REDIS_STRLEN;
                    break;
                }

                if (str6icmp(m, 'z', 'c', 'o', 'u', 'n', 't')) {
                    r->type = MSG_REQ_REDIS_ZCOUNT;
                    break;
                }

                if (str6icmp(m, 'z', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = MSG_REQ_REDIS_ZRANGE;
                    break;
                }

                if (str6icmp(m, 'z', 's', 'c', 'o', 'r', 'e')) {
                    r->type = MSG_REQ_REDIS_ZSCORE;
                    break;
                }

                break;

            case 7:
                if (str7icmp(m, 'p', 'e', 'r', 's', 'i', 's', 't')) {
                    r->type = MSG_REQ_REDIS_PERSIST;
                    break;
                }

                if (str7icmp(m, 'p', 'e', 'x', 'p', 'i', 'r', 'e')) {
                    r->type = MSG_REQ_REDIS_PEXPIRE;
                    break;
                }

                if (str7icmp(m, 'h', 'e', 'x', 'i', 's', 't', 's')) {
                    r->type = MSG_REQ_REDIS_HEXISTS;
                    break;
                }

                if (str7icmp(m, 'h', 'g', 'e', 't', 'a', 'l', 'l')) {
                    r->type = MSG_REQ_REDIS_HGETALL;
                    break;
                }

                if (str7icmp(m, 'h', 'i', 'n', 'c', 'r', 'b', 'y')) {
                    r->type = MSG_REQ_REDIS_HINCRBY;
                    break;
                }

                if (str7icmp(m, 'l', 'i', 'n', 's', 'e', 'r', 't')) {
                    r->type = MSG_REQ_REDIS_LINSERT;
                    break;
                }

                if (str7icmp(m, 'z', 'i', 'n', 'c', 'r', 'b', 'y')) {
                    r->type = MSG_REQ_REDIS_ZINCRBY;
                    break;
                }

                break;

            case 8:
                if (str8icmp(m, 'e', 'x', 'p', 'i', 'r', 'e', 'a', 't')) {
                    r->type = MSG_REQ_REDIS_EXPIREAT;
                    break;
                }

                if (str8icmp(m, 'b', 'i', 't', 'c', 'o', 'u', 'n', 't')) {
                    r->type = MSG_REQ_REDIS_BITCOUNT;
                    break;
                }

                if (str8icmp(m, 'g', 'e', 't', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = MSG_REQ_REDIS_GETRANGE;
                    break;
                }

                if (str8icmp(m, 's', 'e', 't', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = MSG_REQ_REDIS_SETRANGE;
                    break;
                }

                if (str8icmp(m, 's', 'm', 'e', 'm', 'b', 'e', 'r', 's')) {
                    r->type = MSG_REQ_REDIS_SMEMBERS;
                    break;
                }

                if (str8icmp(m, 'z', 'r', 'e', 'v', 'r', 'a', 'n', 'k')) {
                    r->type = MSG_REQ_REDIS_ZREVRANK;
                    break;
                }

                break;

            case 9:
                if (str9icmp(m, 'p', 'e', 'x', 'p', 'i', 'r', 'e', 'a', 't')) {
                    r->type = MSG_REQ_REDIS_PEXPIREAT;
                    break;
                }

                if (str9icmp(m, 's', 'i', 's', 'm', 'e', 'm', 'b', 'e', 'r')) {
                    r->type = MSG_REQ_REDIS_SISMEMBER;
                    break;
                }

                if (str9icmp(m, 'z', 'r', 'e', 'v', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = MSG_REQ_REDIS_ZREVRANGE;
                    break;
                }

                break;

            case 11:
                if (str11icmp(m, 'i', 'n', 'c', 'r', 'b', 'y', 'f', 'l', 'o', 'a', 't')) {
                    r->type = MSG_REQ_REDIS_INCRBYFLOAT;
                    break;
                }

                if (str11icmp(m, 's', 'r', 'a', 'n', 'd', 'm', 'e', 'm', 'b', 'e', 'r')) {
                    r->type = MSG_REQ_REDIS_SRANDMEMBER;
                    break;
                }

                break;

            case 12:
                if (str12icmp(m, 'h', 'i', 'n', 'c', 'r', 'b', 'y', 'f', 'l', 'o', 'a', 't')) {
                    r->type = MSG_REQ_REDIS_HINCRBYFLOAT;
                    break;
                }


                break;

            case 13:
                if (str13icmp(m, 'z', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 's', 'c', 'o', 'r', 'e')) {
                    r->type = MSG_REQ_REDIS_ZRANGEBYSCORE;
                    break;
                }

                break;

            case 15:
                if (str15icmp(m, 'z', 'r', 'e', 'm', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 'r', 'a', 'n', 'k')) {
                    r->type = MSG_REQ_REDIS_ZREMRANGEBYRANK;
                    break;
                }

                break;

            case 16:
                if (str16icmp(m, 'z', 'r', 'e', 'm', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 's', 'c', 'o', 'r', 'e')) {
                    r->type = MSG_REQ_REDIS_ZREMRANGEBYSCORE;
                    break;
                }

                if (str16icmp(m, 'z', 'r', 'e', 'v', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 's', 'c', 'o', 'r', 'e')) {
                    r->type = MSG_REQ_REDIS_ZREVRANGEBYSCORE;
                    break;
                }

                break;

            default:
                break;
            }

            if (r->type == MSG_UNKNOWN) {
                log_error("parsed unsupported command '%.*s'", p - m, m);
                goto error;
            }

            log_debug(LOG_VERB, "parsed command '%.*s'", p - m, m);

            state = SW_REQ_TYPE_LF;
            break;

        case SW_REQ_TYPE_LF:
            switch (ch) {
            case LF:
                state = SW_KEY_LEN;
                break;

            default:
                goto error;
            }

            break;

        case SW_KEY_LEN:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                r->token = p;
                r->rlen = 0;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if (r->rlen == 0) {
                    log_error("rejecting command with empty key");
                    goto error;
                }
                if (r->rlen > 512) {
                    log_error("reject command with long key");
                    goto error;
                }
                if (r->rnarg == 0) {
                    goto error;
                }
                r->rnarg--;
                r->token = NULL;
                state = SW_KEY_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_KEY_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_KEY;
                break;

            default:
                goto error;
            }

            break;

        case SW_KEY:
            if (r->token == NULL) {
                r->token = p;
            }

            m = p + r->rlen;
            if (m >= b->last) {
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            r->rlen = 0;
            m = r->token;
            r->token = NULL;

            r->key_start = m;
            r->key_end = p;

            state = SW_KEY_LF;

            break;

        case SW_KEY_LF:
            switch (ch) {
            case LF:
                if (parse_arg0(r)) {
                    if (r->rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (parse_arg1(r)) {
                    if (r->rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (parse_arg2(r)) {
                    if (r->rnarg != 2) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (parse_arg3(r)) {
                    if (r->rnarg != 3) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (parse_argn(r)) {
                    if (r->rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARG1_LEN;
                } else if (parse_argx(r)) {
                    if (r->rnarg == 0) {
                        goto done;
                    }
                    state = SW_FRAGMENT;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_FRAGMENT:
            r->token = p;
            goto fragment;

        case SW_ARG1_LEN:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                r->rlen = 0;
                r->token = p;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - r->token) <= 1 || r->rnarg == 0) {
                    goto error;
                }
                r->rnarg--;
                r->token = NULL;
                state = SW_ARG1_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG1_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG1;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG1:
            m = p + r->rlen;
            if (m >= b->last) {
                r->rlen -= (uint32_t)(b->last - p);
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            r->rlen = 0;

            state = SW_ARG1_LF;

            break;

        case SW_ARG1_LF:
            switch (ch) {
            case LF:
                if (parse_arg1(r)) {
                    if (r->rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (parse_arg2(r)) {
                    if (r->rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (parse_arg3(r)) {
                    if (r->rnarg != 2) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (parse_argn(r)) {
                    if (r->rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG2_LEN:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                r->rlen = 0;
                r->token = p;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - r->token) <= 1 || r->rnarg == 0) {
                    goto error;
                }
                r->rnarg--;
                r->token = NULL;
                state = SW_ARG2_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG2_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG2;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG2:
            m = p + r->rlen;
            if (m >= b->last) {
                r->rlen -= (uint32_t)(b->last - p);
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            r->rlen = 0;
            state = SW_ARG2_LF;

            break;

        case SW_ARG2_LF:
            switch (ch) {
            case LF:
                if (parse_arg2(r)) {
                    if (r->rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (parse_arg3(r)) {
                    if (r->rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG3_LEN;
                } else if (parse_argn(r)) {
                    if (r->rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG3_LEN:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                r->rlen = 0;
                r->token = p;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - r->token) <= 1 || r->rnarg == 0) {
                    goto error;
                }
                r->rnarg--;
                r->token = NULL;
                state = SW_ARG3_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG3_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG3;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG3:
            m = p + r->rlen;
            if (m >= b->last) {
                r->rlen -= (uint32_t)(b->last - p);
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            r->rlen = 0;
            state = SW_ARG3_LF;

            break;

        case SW_ARG3_LF:
            switch (ch) {
            case LF:
                if (parse_arg3(r)) {
                    if (r->rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (parse_argn(r)) {
                    if (r->rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARGN_LEN:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                r->rlen = 0;
                r->token = p;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - r->token) <= 1 || r->rnarg == 0) {
                    goto error;
                }
                r->rnarg--;
                r->token = NULL;
                state = SW_ARGN_LEN_LF;
            }  else {
                goto error;
            }

            break;

        case SW_ARGN_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARGN;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARGN:
            m = p + r->rlen;
            if (m >= b->last) {
                r->rlen -= (uint32_t)(b->last - p);
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            r->rlen = 0;
            state = SW_ARGN_LF;

            break;

        case SW_ARGN_LF:
            switch (ch) {
            case LF:
                if (parse_argn(r)) {
                    if (r->rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_SENTINEL:
        default:
            NOT_REACHED();
            break;
        }
    }

    ASSERT(p == b->last);
    r->pos = p;
    r->state = state;

    if (b->last == b->end && r->token != NULL) {
        r->pos = r->token;
        r->token = NULL;
        r->result = PARSE_REPAIR;
    } else {
        r->result = PARSE_AGAIN;
    }

    log_hexdump(LOG_VERB, b->pos, mbuf_length(b), "parsed req %"PRIu64" res %d "
                "type %d state %d rpos %d of %d", r->id, r->result, r->type,
                r->state, r->pos - b->pos, b->last - b->pos);
    return;

fragment:
    ASSERT(p != b->last);
    ASSERT(r->token != NULL);
    r->pos = r->token;
    r->token = NULL;
    r->state = state;
    r->result = PARSE_FRAGMENT;

    log_hexdump(LOG_VERB, b->pos, mbuf_length(b), "parsed req %"PRIu64" res %d "
               "type %d state %d rpos %d of %d", r->id, r->result, r->type,
               r->state, r->pos - b->pos, b->last - b->pos);
    return;

done:
    ASSERT(r->type > MSG_UNKNOWN && r->type < MSG_SENTINEL);
    r->pos = p + 1;
    ASSERT(r->pos <= b->last);
    r->state = SW_START;
    r->token = NULL;
    r->result = PARSE_OK;

    log_hexdump(LOG_VERB, b->pos, mbuf_length(b), "parsed req %"PRIu64" res %d "
                "type %d state %d rpos %d of %d", r->id, r->result, r->type,
                r->state, r->pos - b->pos, b->last - b->pos);
    return;

error:
    r->result = PARSE_ERROR;
    r->state = state;
    errno = EINVAL;

    log_hexdump(LOG_INFO, b->pos, mbuf_length(b), "parsed bad req %"PRIu64" "
                "res %d type %d state %d", r->id, r->result, r->type,
                r->state);
}

/*
 * Reference: http://redis.io/topics/protocol
 *
 * Redis will reply to commands with different kinds of replies. It is
 * possible to check the kind of reply from the first byte sent by the
 * server:
 *  - with a single line reply the first byte of the reply will be "+"
 *  - with an error message the first byte of the reply will be "-"
 *  - with an integer number the first byte of the reply will be ":"
 *  - with bulk reply the first byte of the reply will be "$"
 *  - with multi-bulk reply the first byte of the reply will be "*"
 *
 * 1). Status reply (or single line reply) is in the form of a single line
 *     string starting with "+" terminated by "\r\n".
 * 2). Error reply are similar to status replies. The only difference is
 *     that the first byte is "-" instead of "+".
 * 3). Integer reply is just a CRLF terminated string representing an
 *     integer, and prefixed by a ":" byte.
 * 4). Bulk reply is used by server to return a single binary safe string.
 *     The first reply line is a "$" byte followed by the number of bytes
 *     of the the actual reply, followed by CRLF, then the actual data bytes,
 *     followed by additional two bytes for the final CRLF. If the requested
 *     value does not exist the bulk reply will use the special value '-1'
 *     as the data length.
 * 5). Multi-bulk reply is used by the server to return many binary safe
 *     strings (bulks) with the intial line indicating how many bulks that
 *     will follow. The first byte of a multi bulk reply is always *.
 */
void
parse_response(struct msg *r)
{
    struct mbuf *b;
    uint8_t *p, *m;
    uint8_t ch;
    enum {
        SW_START,
        SW_STATUS,
        SW_ERROR,
        SW_INTEGER,
        SW_INTEGER_START,
        SW_BULK,
        SW_BULK_LF,
        SW_BULK_ARG,
        SW_BULK_ARG_LF,
        SW_MULTIBULK,
        SW_MULTIBULK_NARG_LF,
        SW_MULTIBULK_ARGN_LEN,
        SW_MULTIBULK_ARGN_LEN_LF,
        SW_MULTIBULK_ARGN,
        SW_MULTIBULK_ARGN_LF,
        SW_RUNTO_CRLF,
        SW_ALMOST_DONE,
        SW_SENTINEL
    } state;

    state = r->state;
    b = STAILQ_LAST(&r->mhdr, mbuf, next);

    ASSERT(!r->request);
    ASSERT(state >= SW_START && state < SW_SENTINEL);
    ASSERT(b != NULL);
    ASSERT(b->pos <= b->last);

    /* validate the parsing marker */
    ASSERT(r->pos != NULL);
    ASSERT(r->pos >= b->pos && r->pos <= b->last);

    for (p = r->pos; p < b->last; p++) {
        ch = *p;

        switch (state) {
        case SW_START:
            r->type = MSG_UNKNOWN;
            switch (ch) {
            case '+':
                p = p - 1; /* go back by 1 byte */
                r->type = MSG_RSP_REDIS_STATUS;
                state = SW_STATUS;
                break;

            case '-':
                r->type = MSG_RSP_REDIS_ERROR;
                p = p - 1; /* go back by 1 byte */
                state = SW_ERROR;
                break;

            case ':':
                r->type = MSG_RSP_REDIS_INTEGER;
                p = p - 1; /* go back by 1 byte */
                state = SW_INTEGER;
                break;

            case '$':
                r->type = MSG_RSP_REDIS_BULK;
                p = p - 1; /* go back by 1 byte */
                state = SW_BULK;
                break;

            case '*':
                r->type = MSG_RSP_REDIS_MULTIBULK;
                p = p - 1; /* go back by 1 byte */
                state = SW_MULTIBULK;
                break;

            default:
                goto error;
            }

            break;

        case SW_STATUS:
            /* rsp_start <- p */
            state = SW_RUNTO_CRLF;
            break;

        case SW_ERROR:
            /* rsp_start <- p */
            state = SW_RUNTO_CRLF;
            break;

        case SW_INTEGER:
            /* rsp_start <- p */
            state = SW_INTEGER_START;
            r->integer = 0;
            break;

        case SW_INTEGER_START:
            if (ch == CR) {
                state = SW_ALMOST_DONE;
            } else if (ch == '-') {
                ;
            } else if (isdigit(ch)) {
                r->integer = r->integer * 10 + (uint32_t)(ch - '0');
            } else {
                goto error;
            }
            break;

        case SW_RUNTO_CRLF:
            switch (ch) {
            case CR:
                state = SW_ALMOST_DONE;
                break;

            default:
                break;
            }

            break;

        case SW_ALMOST_DONE:
            switch (ch) {
            case LF:
                /* rsp_end <- p */
                goto done;

            default:
                goto error;
            }

            break;

        case SW_BULK:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                /* rsp_start <- p */
                r->token = p;
                r->rlen = 0;
            } else if (ch == '-') {
                state = SW_RUNTO_CRLF;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - r->token) <= 1) {
                    goto error;
                }
                r->token = NULL;
                state = SW_BULK_LF;
            } else {
                goto error;
            }

            break;

        case SW_BULK_LF:
            switch (ch) {
            case LF:
                state = SW_BULK_ARG;
                break;

            default:
                goto error;
            }

            break;

        case SW_BULK_ARG:
            m = p + r->rlen;
            if (m >= b->last) {
                r->rlen -= (uint32_t)(b->last - p);
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            r->rlen = 0;

            state = SW_BULK_ARG_LF;

            break;

        case SW_BULK_ARG_LF:
            switch (ch) {
            case LF:
                goto done;

            default:
                goto error;
            }

            break;

        case SW_MULTIBULK:
            if (r->token == NULL) {
                if (ch != '*') {
                    goto error;
                }
                r->token = p;
                /* rsp_start <- p */
                r->narg_start = p;
                r->rnarg = 0;
            } else if (isdigit(ch)) {
                r->rnarg = r->rnarg * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - r->token) <= 1) {
                    goto error;
                }
                r->narg = r->rnarg;
                r->narg_end = p;
                r->token = NULL;
                state = SW_MULTIBULK_NARG_LF;
            } else {
                goto error;
            }

            break;

        case SW_MULTIBULK_NARG_LF:
            switch (ch) {
            case LF:
                if (r->rnarg == 0) {
                    /* response is '*0\r\n' */
                    goto done;
                }
                state = SW_MULTIBULK_ARGN_LEN;
                break;

            default:
                goto error;
            }

            break;

        case SW_MULTIBULK_ARGN_LEN:
            if (r->token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                r->token = p;
                r->rlen = 0;
            } else if (isdigit(ch)) {
                r->rlen = r->rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == '-') {
                ;
            } else if (ch == CR) {
                if ((p - r->token) <= 1 || r->rnarg == 0) {
                    goto error;
                }

                if (r->rlen == 1 && (p - r->token) == 3) {
                    /* handles not-found reply = '$-1'*/
                    r->rlen = 0;
                    state = SW_MULTIBULK_ARGN_LF;
                } else {
                    state = SW_MULTIBULK_ARGN_LEN_LF;
                }
                r->rnarg--;
                r->token = NULL;

            } else {
                goto error;
            }

            break;

        case SW_MULTIBULK_ARGN_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_MULTIBULK_ARGN;
                break;

            default:
                goto error;
            }

            break;

        case SW_MULTIBULK_ARGN:
            m = p + r->rlen;
            if (m >= b->last) {
                r->rlen -= (uint32_t)(b->last - p);
                m = b->last - 1;
                p = m;
                break;
            }

            if (*m != CR) {
                goto error;
            }

            p += r->rlen; /* move forward by rlen bytes */
            r->rlen = 0;

            state = SW_MULTIBULK_ARGN_LF;

            break;

        case SW_MULTIBULK_ARGN_LF:
            switch (ch) {
            case LF:
                if (r->rnarg == 0) {
                    goto done;
                }

                state = SW_MULTIBULK_ARGN_LEN;
                break;

            default:
                goto error;
            }

            break;

        case SW_SENTINEL:
        default:
            NOT_REACHED();
            break;
        }
    }

    ASSERT(p == b->last);
    r->pos = p;
    r->state = state;

    if (b->last == b->end && r->token != NULL) {
        r->pos = r->token;
        r->token = NULL;
        r->result = PARSE_REPAIR;
    } else {
        r->result = PARSE_AGAIN;
    }

    log_hexdump(LOG_VERB, b->pos, mbuf_length(b), "parsed rsp %"PRIu64" res %d "
                "type %d state %d rpos %d of %d", r->id, r->result, r->type,
                r->state, r->pos - b->pos, b->last - b->pos);
    return;

done:
    ASSERT(r->type > MSG_UNKNOWN && r->type < MSG_SENTINEL);
    r->pos = p + 1;
    ASSERT(r->pos <= b->last);
    r->state = SW_START;
    r->token = NULL;
    r->result = PARSE_OK;

    log_hexdump(LOG_VERB, b->pos, mbuf_length(b), "parsed rsp %"PRIu64" res %d "
                "type %d state %d rpos %d of %d", r->id, r->result, r->type,
                r->state, r->pos - b->pos, b->last - b->pos);
    return;

error:
    r->result = PARSE_ERROR;
    r->state = state;
    errno = EINVAL;

    log_hexdump(LOG_INFO, b->pos, mbuf_length(b), "parsed bad rsp %"PRIu64" "
                "res %d type %d state %d", r->id, r->result, r->type,
                r->state);
}

void
redis_fixup(struct msg *msg)
{
    struct msg *pmsg = msg->peer; /* peer request */
    struct mbuf *mbuf;

    ASSERT(!msg->request);
    ASSERT(pmsg->request);

    if (pmsg->frag_id == 0) {
        return;
    }

    /*
     * Valid responses for a fragmented requests are MSG_RSP_REDIS_INTEGER or,
     * MSG_RSP_REDIS_MULTIBULK. For an invalid response, we send out -ERR
     * with EINVAL errno
     */
    if (msg->type != MSG_RSP_REDIS_INTEGER && msg->type != MSG_RSP_REDIS_MULTIBULK) {
        mbuf = STAILQ_FIRST(&msg->mhdr);
        log_hexdump(LOG_ERR, mbuf->pos, mbuf_length(mbuf), "rsp fragment "
                    "with unknown type %d", msg->type);
        pmsg->error = 1;
        pmsg->err = EINVAL;
        return;
    }

    if (msg->type == MSG_RSP_REDIS_INTEGER) {
        ASSERT(pmsg->type == MSG_REQ_REDIS_DEL);
        /*
         * Only redis 'del' command is a candidate for fragmentation
         * and sends back a integer reply.
         *
         * Because of how we parse replies, the integer reply will be
         * completely encpsuated in a single mbuf and we should skip
         * over all the mbuf contents as the parser has already parsed
         * and stored reply in msg->integer
         */
        mbuf = STAILQ_FIRST(&msg->mhdr);

        ASSERT(mbuf == STAILQ_LAST(&msg->mhdr, mbuf, next));
        ASSERT(msg->mlen == mbuf_length(mbuf));

        msg->mlen -= mbuf_length(mbuf);
        mbuf_rewind(mbuf);

        mbuf->pos = mbuf->start;
        mbuf->last = mbuf->pos;
    }

    if (msg->type == MSG_RSP_REDIS_MULTIBULK) {
        ASSERT(pmsg->type == MSG_REQ_REDIS_MGET);
        /*
         * Only redis 'mget' command is a candidate for fragmentation
         * and sends back a multi-bulk reply
         *
         * The muti-bulk reply can span over multiple mbufs and in each
         * reply we would like to skip over the narg token.
         *
         * Furthermore, because of the way I parse and tokenize replies,
         * the '\r\n' might not exists in a contiguous region.
         */
        mbuf = STAILQ_FIRST(&msg->mhdr);
        ASSERT(msg->narg_start == mbuf->pos);
        msg->narg_end += CRLF_LEN;
        msg->mlen -= (uint32_t) (msg->narg_end - msg->narg_start);
        mbuf->pos = msg->narg_end;
    }
}
