#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	while(1) {
		fprintf(stderr, "hello world\n");
		sleep(10);		
	}
}
