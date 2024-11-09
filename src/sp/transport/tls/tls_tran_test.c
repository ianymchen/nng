//
// Copyright 2024 Staysail Systems, Inc. <info@staysail.tech>
// Copyright 2018 Capitar IT Group BV <info@capitar.com>
// Copyright 2018 Devolutions <info@devolutions.net>
// Copyright 2018 Cody Piersall <cody.piersall@gmail.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "nng/nng.h"
#include "nng/supplemental/tls/tls.h"
#include <nuts.h>

// TLS tests.

static nng_tls_config *
tls_server_config(void)
{
	nng_tls_config *c;
	NUTS_PASS(nng_tls_config_alloc(&c, NNG_TLS_MODE_SERVER));
	NUTS_PASS(nng_tls_config_own_cert(
	    c, nuts_server_crt, nuts_server_key, NULL));
	return (c);
}

static nng_tls_config *
tls_config_psk(nng_tls_mode mode, const char *name, uint8_t *key, size_t len)
{
	nng_tls_config *c;
	NUTS_PASS(nng_tls_config_alloc(&c, mode));
	NUTS_PASS(nng_tls_config_psk(c, name, key, len));
	return (c);
}

static nng_tls_config *
tls_client_config(void)
{
	nng_tls_config *c;
	NUTS_PASS(nng_tls_config_alloc(&c, NNG_TLS_MODE_CLIENT));
	NUTS_PASS(nng_tls_config_own_cert(
	    c, nuts_client_crt, nuts_client_key, NULL));
	NUTS_PASS(nng_tls_config_ca_chain(c, nuts_server_crt, NULL));
	return (c);
}

static void
test_tls_wild_card_connect_fail(void)
{
	nng_socket s;
	nng_dialer d;
	char       addr[NNG_MAXADDRLEN];

	NUTS_OPEN(s);
	(void) snprintf(
	    addr, sizeof(addr), "tls+tcp://*:%u", nuts_next_port());
	NUTS_FAIL(nng_dialer_create(&d, s, addr), NNG_EADDRINVAL);
	NUTS_CLOSE(s);
}

void
test_tls_wild_card_bind(void)
{
	nng_socket      s1;
	nng_socket      s2;
	nng_listener    l;
	nng_dialer      d;
	char            addr[NNG_MAXADDRLEN];
	uint16_t        port;
	nng_tls_config *cc;
	nng_tls_config *sc;
	nng_tls_config *other;

	port = nuts_next_port();

	sc = tls_server_config();
	cc = tls_client_config();

	NUTS_OPEN(s1);
	NUTS_OPEN(s2);
	(void) snprintf(addr, sizeof(addr), "tls+tcp4://*:%u", port);
	NUTS_PASS(nng_listener_create(&l, s1, addr));
	NUTS_PASS(nng_listener_set_tls(l, sc));
	NUTS_PASS(nng_listener_get_tls(l, &other));
	NUTS_TRUE(sc == other);
	NUTS_PASS(nng_listener_start(l, 0));
	(void) snprintf(addr, sizeof(addr), "tls+tcp://127.0.0.1:%u", port);
	NUTS_PASS(nng_dialer_create(&d, s2, addr));
	NUTS_PASS(nng_dialer_set_tls(d, cc));
	NUTS_PASS(nng_dialer_get_tls(d, &other));
	NUTS_TRUE(cc == other);
	NUTS_PASS(nng_dialer_start(d, 0));
	NUTS_CLOSE(s2);
	NUTS_CLOSE(s1);
	nng_tls_config_free(cc);
	nng_tls_config_free(sc);
}

void
test_tls_port_zero_bind(void)
{
	nng_socket      s1;
	nng_socket      s2;
	nng_tls_config *c1, *c2;
	nng_sockaddr    sa;
	nng_listener    l;
	nng_dialer      d;
	char           *addr;

	c1 = tls_server_config();
	c2 = tls_client_config();
	NUTS_OPEN(s1);
	NUTS_OPEN(s2);
	NUTS_PASS(nng_listener_create(&l, s1, "tls+tcp://127.0.0.1:0"));
	NUTS_PASS(nng_listener_set_tls(l, c1));
	NUTS_PASS(nng_listener_start(l, 0));
	NUTS_PASS(nng_listener_get_string(l, NNG_OPT_URL, &addr));
	NUTS_TRUE(memcmp(addr, "tls+tcp://", 6) == 0);
	NUTS_PASS(nng_listener_get_addr(l, NNG_OPT_LOCADDR, &sa));
	NUTS_TRUE(sa.s_in.sa_family == NNG_AF_INET);
	NUTS_TRUE(sa.s_in.sa_port != 0);
	NUTS_TRUE(sa.s_in.sa_addr = nuts_be32(0x7f000001));
	NUTS_PASS(nng_dialer_create(&d, s2, addr));
	NUTS_PASS(nng_dialer_set_tls(d, c2));
	NUTS_PASS(nng_dialer_start(d, 0));
	nng_strfree(addr);
	NUTS_CLOSE(s2);
	NUTS_CLOSE(s1);
	nng_tls_config_free(c1);
	nng_tls_config_free(c2);
}

void
test_tls_local_address_connect(void)
{

	nng_socket      s1;
	nng_socket      s2;
	nng_tls_config *c1, *c2;
	nng_dialer      d;
	nng_listener    l;
	char            addr[NNG_MAXADDRLEN];
	uint16_t        port;

	c1 = tls_server_config();
	c2 = tls_client_config();
	NUTS_OPEN(s1);
	NUTS_OPEN(s2);
	port = nuts_next_port();
	(void) snprintf(addr, sizeof(addr), "tls+tcp://127.0.0.1:%u", port);
	NUTS_PASS(nng_listener_create(&l, s1, addr));
	NUTS_PASS(nng_listener_set_tls(l, c1));
	NUTS_PASS(nng_listener_start(l, 0));
	(void) snprintf(
	    addr, sizeof(addr), "tls+tcp://127.0.0.1;127.0.0.1:%u", port);
	NUTS_PASS(nng_dialer_create(&d, s2, addr));
	NUTS_PASS(nng_dialer_set_tls(d, c2));
	NUTS_PASS(nng_dialer_start(d, 0));
	NUTS_CLOSE(s2);
	NUTS_CLOSE(s1);
	nng_tls_config_free(c1);
	nng_tls_config_free(c2);
}

void
test_tls_malformed_address(void)
{
	nng_socket s1;

	NUTS_OPEN(s1);
	NUTS_FAIL(
	    nng_dial(s1, "tls+tcp://127.0.0.1", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(
	    nng_dial(s1, "tls+tcp://127.0.0.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(
	    nng_dial(s1, "tls+tcp://127.0.x.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(
	    nng_listen(s1, "tls+tcp://127.0.0.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(
	    nng_listen(s1, "tls+tcp://127.0.x.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_CLOSE(s1);
}

void
test_tls_no_delay_option(void)
{
	nng_socket      s;
	nng_dialer      d;
	nng_listener    l;
	bool            v;
	int             x;
	char           *addr;
	nng_tls_config *dc, *lc;

	NUTS_ADDR(addr, "tls+tcp");
	dc = tls_client_config();
	lc = tls_server_config();

	NUTS_OPEN(s);
	NUTS_PASS(nng_dialer_create(&d, s, addr));
	NUTS_PASS(nng_dialer_set_tls(d, dc));
	NUTS_PASS(nng_dialer_get_bool(d, NNG_OPT_TCP_NODELAY, &v));
	NUTS_TRUE(v);
	NUTS_PASS(nng_dialer_set_bool(d, NNG_OPT_TCP_NODELAY, false));
	NUTS_PASS(nng_dialer_get_bool(d, NNG_OPT_TCP_NODELAY, &v));
	NUTS_TRUE(v == false);
	NUTS_FAIL(
	    nng_dialer_get_int(d, NNG_OPT_TCP_NODELAY, &x), NNG_EBADTYPE);
	x = 0;
	NUTS_FAIL(nng_dialer_set_int(d, NNG_OPT_TCP_NODELAY, x), NNG_EBADTYPE);

	NUTS_PASS(nng_listener_create(&l, s, addr));
	NUTS_PASS(nng_listener_set_tls(l, lc));
	NUTS_PASS(nng_listener_get_bool(l, NNG_OPT_TCP_NODELAY, &v));
	NUTS_TRUE(v == true);
	x = 0;
	NUTS_FAIL(
	    nng_listener_set_int(l, NNG_OPT_TCP_NODELAY, x), NNG_EBADTYPE);

	NUTS_PASS(nng_dialer_close(d));
	NUTS_PASS(nng_listener_close(l));

	NUTS_CLOSE(s);
	nng_tls_config_free(lc);
	nng_tls_config_free(dc);
}

void
test_tls_keep_alive_option(void)
{
	nng_socket      s;
	nng_dialer      d;
	nng_listener    l;
	nng_tls_config *dc, *lc;
	bool            v;
	int             x;
	char           *addr;

	dc = tls_client_config();
	lc = tls_server_config();
	NUTS_ADDR(addr, "tls+tcp");
	NUTS_OPEN(s);
	NUTS_PASS(nng_dialer_create(&d, s, addr));
	NUTS_PASS(nng_dialer_set_tls(d, dc));
	NUTS_PASS(nng_dialer_get_bool(d, NNG_OPT_TCP_KEEPALIVE, &v));
	NUTS_TRUE(v == false);
	NUTS_PASS(nng_dialer_set_bool(d, NNG_OPT_TCP_KEEPALIVE, true));
	NUTS_PASS(nng_dialer_get_bool(d, NNG_OPT_TCP_KEEPALIVE, &v));
	NUTS_TRUE(v);
	NUTS_FAIL(
	    nng_dialer_get_int(d, NNG_OPT_TCP_KEEPALIVE, &x), NNG_EBADTYPE);
	x = 1;
	NUTS_FAIL(
	    nng_dialer_set_int(d, NNG_OPT_TCP_KEEPALIVE, x), NNG_EBADTYPE);

	NUTS_PASS(nng_listener_create(&l, s, addr));
	NUTS_PASS(nng_listener_set_tls(l, lc));
	NUTS_PASS(nng_listener_get_bool(l, NNG_OPT_TCP_KEEPALIVE, &v));
	NUTS_TRUE(v == false);
	x = 1;
	NUTS_FAIL(
	    nng_listener_set_int(l, NNG_OPT_TCP_KEEPALIVE, x), NNG_EBADTYPE);

	NUTS_PASS(nng_dialer_close(d));
	NUTS_PASS(nng_listener_close(l));

	NUTS_CLOSE(s);
	nng_tls_config_free(lc);
	nng_tls_config_free(dc);
}

void
test_tls_recv_max(void)
{
	char            msg[256];
	char            buf[256];
	nng_socket      s0;
	nng_socket      s1;
	nng_tls_config *c0, *c1;
	nng_listener    l;
	nng_dialer      d;
	size_t          sz;
	char           *addr;

	NUTS_ADDR(addr, "tls+tcp");

	c0 = tls_server_config();
	c1 = tls_client_config();
	NUTS_OPEN(s0);
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_socket_set_size(s0, NNG_OPT_RECVMAXSZ, 200));
	NUTS_PASS(nng_listener_create(&l, s0, addr));
	NUTS_PASS(nng_listener_set_tls(l, c0));
	NUTS_PASS(nng_socket_get_size(s0, NNG_OPT_RECVMAXSZ, &sz));
	NUTS_TRUE(sz == 200);
	NUTS_PASS(nng_listener_set_size(l, NNG_OPT_RECVMAXSZ, 100));
	NUTS_PASS(nng_listener_start(l, 0));

	NUTS_OPEN(s1);
	NUTS_PASS(nng_dialer_create(&d, s1, addr));
	NUTS_PASS(nng_dialer_set_tls(d, c1));
	NUTS_PASS(nng_dialer_start(d, 0));
	NUTS_PASS(nng_send(s1, msg, 95, 0));
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_SENDTIMEO, 100));
	NUTS_PASS(nng_recv(s0, buf, &sz, 0));
	NUTS_TRUE(sz == 95);
	NUTS_PASS(nng_send(s1, msg, 150, 0));
	NUTS_FAIL(nng_recv(s0, buf, &sz, 0), NNG_ETIMEDOUT);
	NUTS_PASS(nng_close(s0));
	NUTS_CLOSE(s1);
	nng_tls_config_free(c0);
	nng_tls_config_free(c1);
}

void
test_tls_psk(void)
{
#ifdef NNG_SUPP_TLS_PSK
	char            msg[256];
	char            buf[256];
	nng_socket      s0;
	nng_socket      s1;
	nng_tls_config *c0, *c1;
	nng_listener    l;
	nng_dialer      d;
	size_t          sz;
	char           *addr;
	uint8_t         key[32];

	for (unsigned i = 0; i < sizeof(key); i++) {
		key[i] = rand() % 0xff;
	}

	NUTS_ADDR(addr, "tls+tcp");

	c0 = tls_config_psk(NNG_TLS_MODE_SERVER, "identity", key, sizeof key);
	c1 = tls_config_psk(NNG_TLS_MODE_CLIENT, "identity", key, sizeof key);
	NUTS_OPEN(s0);
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_listener_create(&l, s0, addr));
	NUTS_PASS(nng_listener_set_tls(l, c0));
	NUTS_PASS(nng_listener_start(l, 0));

	NUTS_OPEN(s1);
	NUTS_PASS(nng_dialer_create(&d, s1, addr));
	NUTS_PASS(nng_dialer_set_tls(d, c1));
	NUTS_PASS(nng_dialer_start(d, 0));
	NUTS_PASS(nng_send(s1, msg, 95, 0));
	NUTS_PASS(nng_recv(s0, buf, &sz, 0));
	NUTS_TRUE(sz == 95);
	NUTS_PASS(nng_close(s0));
	NUTS_CLOSE(s1);
	nng_tls_config_free(c0);
	nng_tls_config_free(c1);
#else
	NUTS_SKIP("no PSK support");
#endif
}

NUTS_TESTS = {

	{ "tls wild card connect fail", test_tls_wild_card_connect_fail },
	{ "tls wild card bind", test_tls_wild_card_bind },
	{ "tls port zero bind", test_tls_port_zero_bind },
	{ "tls local address connect", test_tls_local_address_connect },
	{ "tls malformed address", test_tls_malformed_address },
	{ "tls no delay option", test_tls_no_delay_option },
	{ "tls keep alive option", test_tls_keep_alive_option },
	{ "tls recv max", test_tls_recv_max },
	{ "tls pre-shared key", test_tls_psk },
	{ NULL, NULL },
};
