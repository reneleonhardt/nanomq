//
// Copyright 2023 NanoMQ Team, Inc. <jaylin@emqx.io>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//
#include "include/broker.h"
#include "tests_api.h"

int
main()
{
	if (!test_env_allows_network_binds()) {
		fprintf(stderr, "skip: test environment disallows listening sockets\n");
		return 0;
	}
	if (!test_env_connects_to_host(
	            "a2zegtl0x5owup-ats.iot.ap-northeast-1.amazonaws.com",
	            "8883")) {
		fprintf(stderr,
	            "skip: AWS host unavailable from this test environment\n");
		return 0;
	}
	if (!test_env_has_executable("mosquitto_sub") ||
	    !test_env_has_executable("mosquitto_pub") ||
	    !test_env_has_file("../../../nanomq/tests/aws-key.pem") ||
	    !test_env_has_file("../../../nanomq/tests/aws-cert.pem") ||
	    !test_env_has_file("../../../nanomq/tests/aws-cacert.pem") ||
	    !test_env_connects_to_host("broker.emqx.io", "1883")) {
		fprintf(stderr,
		    "skip: external bridge clients, AWS TLS credentials, or broker unavailable\n");
		return 0;
	}

	char *cmd = "mosquitto_sub";
	const char *test_port = test_env_test_port_text();

	char *cmd_sub[] = {"mosquitto_sub", "-h", "127.0.0.1", "-p", (char *) test_port, "-t", "nmqtest_sub", "-V", "mqttv5", NULL};
	char cmd_pub[128];

	char *cmd_sub_emqx[] = {"mosquitto_sub", "-h", "broker.emqx.io", "-p", "1883", "-t", "fwd1/test/ci", "-V", "mqttv5", "-q", "2", NULL};
	char *cmd_sub_nmq[] = {"mosquitto_sub", "-h", "127.0.0.1", "-p", (char *) test_port, "-t", "recv_lo/topic1", "-V", "mqttv5", "-q", "2", NULL};
	char cmd_pub_nmq[192];
	char *cmd_pub_emqx = "mosquitto_pub -h broker.emqx.io -p 1883 -t recv/topic1/ci -m message-to-nmq -V mqttv5 -q 2 -r";

	nng_thread *nmq;
	pid_t       pid_sub;
	pid_t       pid_sub_nmq;
	pid_t       pid_sub_emqx;
	conf       *conf       = NULL;
	FILE       *p_pub      = NULL;
	FILE       *p_pub_nmq  = NULL;
	FILE       *p_pub_emqx = NULL;

	int buf_size = 128;
	int  outfp;
	char buf[buf_size];
	memset(buf, 0, buf_size);

	int  outfp_nmq, outfp_emqx;
	char buf_nmq[buf_size];
	char buf_emqx[buf_size];
	memset(buf_nmq, 0, buf_size);
	memset(buf_emqx, 0, buf_size);

	// create nmq thread
	conf = get_test_conf(BRIDGE_MUTI_CONF);
	assert(conf != NULL);
	nng_thread_create(&nmq, (void *) broker_start_with_conf, (void *) conf);
	nng_msleep(1000); // wait a while before sub
	snprintf(cmd_pub, sizeof(cmd_pub),
	    "mosquitto_pub -h 127.0.0.1 -p %s -t nmqtest_lo -m message-to-aws -V mqttv5 -r",
	    test_port);
	snprintf(cmd_pub_nmq, sizeof(cmd_pub_nmq),
	    "mosquitto_pub -h 127.0.0.1 -p %s -t forward1/test/ci -m message-to-emqx -V mqttv5 -q 2 -r",
	    test_port);
	pid_sub = popen_with_cmd(&outfp, cmd_sub, cmd);
	pid_sub_nmq = popen_with_cmd(&outfp_nmq, cmd_sub_nmq, cmd);
	pid_sub_emqx = popen_with_cmd(&outfp_emqx, cmd_sub_emqx, cmd);
	nng_msleep(2000);
	p_pub = popen(cmd_pub, "r");
	p_pub_emqx = popen(cmd_pub_emqx, "r");
	p_pub_nmq= popen(cmd_pub_nmq, "r");
	assert(p_pub != NULL);
	assert(p_pub_emqx != NULL);
	assert(p_pub_nmq != NULL);
	// check recv msg
	memset(buf, 0, buf_size);
	assert(test_env_wait_for_output(outfp, buf, buf_size, 8000, 50));
	printf("get the msg in nmq:%s\n", buf);
	assert(strncmp(buf, "message-to-aws", 14) == 0);

	memset(buf_nmq, 0, buf_size);
	assert(test_env_wait_for_output(outfp_nmq, buf_nmq, buf_size, 8000, 50));
	printf("get the msg in nmq:%s\n", buf_nmq);
	assert(strncmp(buf_nmq, "message-to-nmq", 14) == 0);
	memset(buf_emqx, 0, buf_size);
	assert(test_env_wait_for_output(outfp_emqx, buf_emqx, buf_size, 8000, 50));
	printf("get the msg in emqx:%s\n", buf_emqx);
	assert(strncmp(buf_emqx, "message-to-emqx", 15) == 0);

	kill(pid_sub, SIGKILL);
	assert(pclose(p_pub) == 0);
	close(outfp);

	kill(pid_sub_nmq, SIGKILL);
	kill(pid_sub_emqx, SIGKILL);
	assert(pclose(p_pub_nmq) == 0);
	assert(pclose(p_pub_emqx) == 0);
	close(outfp_nmq);
	close(outfp_emqx);

	broker_stop_for_test();
	nng_thread_destroy(nmq);

	return 0;
}
