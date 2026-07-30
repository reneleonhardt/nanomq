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
	    !test_env_has_file("../../../nanomq/tests/aws-cacert.pem")) {
		fprintf(stderr,
		    "skip: MQTT clients or AWS TLS credentials not available\n");
		return 0;
	}

	char *cmd = "mosquitto_sub";
	const char *test_port = test_env_test_port_text();

	char *cmd_sub[] = {"mosquitto_sub", "-h", "127.0.0.1", "-p", (char *) test_port, "-t", "nmqtest_sub", "-V", "mqttv5", NULL};

	char cmd_pub[128];

	nng_thread *nmq;
	pid_t       pid_sub;
	conf       *conf  = NULL;
	FILE       *p_pub = NULL;

	int buf_size = 128;
	int  outfp;
	char buf[buf_size];
	memset(buf, 0, buf_size);

	// create nmq thread
	conf = get_test_conf(BRIDGE_AWS_CONF);
	assert(conf != NULL);
	nng_thread_create(&nmq, (void *) broker_start_with_conf, (void *) conf);
	nng_msleep(1000); // wait a while before sub
	pid_sub = popen_with_cmd(&outfp, cmd_sub, cmd);
	nng_msleep(2000);
	snprintf(cmd_pub, sizeof(cmd_pub),
	    "mosquitto_pub -h 127.0.0.1 -p %s -t nmqtest_lo -m message-to-aws -V mqttv5",
	    test_port);
	p_pub = popen(cmd_pub, "r");
	assert(p_pub != NULL);
	// check recv msg
	memset(buf, 0, buf_size);
	assert(test_env_wait_for_output(outfp, buf, buf_size, 8000, 50));
	printf("get the msg in nmq:%s\n", buf);
	assert(strncmp(buf, "message-to-aws", 14) == 0);

	kill(pid_sub, SIGKILL);
	assert(pclose(p_pub) == 0);
	close(outfp);
	broker_stop_for_test();
	nng_thread_destroy(nmq);

	return 0;
}
