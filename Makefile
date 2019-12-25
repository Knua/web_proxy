all: web_proxy

web_proxy:
	g++ -o web_proxy web_proxy.cpp -pthread

clean:
	rm -rf *.o web_proxy
