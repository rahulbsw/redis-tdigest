ARG REDIS_VER=latest
FROM redis:${REDIS_VER} as redis-tdigest
RUN set -eux; \
	mkdir -p /usr/src;

COPY . /usr/src/

RUN set -eux; \
	\
	savedAptMark="$(apt-mark showmanual)"; \
	apt-get update; \
	apt-get install -y --no-install-recommends \
		ca-certificates \
		wget \
		\
		dpkg-dev \
		gcc \
		libc6-dev \
		libssl-dev \
		make \
        git \
	; \
	rm -rf /var/lib/apt/lists/*; \
	\
	#mkdir -p /usr/src; \
	mkdir -p /usr/lib/redis/modules/; \
    #cd /usr/src ; \
	#git clone https://github.com/rahulbsw/redis-tdigest \
	#; \
	ls -l /usr/src/redis-tdigest/ ; \
	cd /usr/src/redis-tdigest && make clean && make \
	; \
	cp -r /usr/src/redis-tdigest/tdigest.so /usr/lib/redis/modules/; \
	rm -r /usr/src/redis-tdigest; \
	\
	apt-mark auto '.*' > /dev/null; \
	[ -z "$savedAptMark" ] || apt-mark manual $savedAptMark > /dev/null; \
	find /usr/local -type f -executable -exec ldd '{}' ';' \
		| awk '/=>/ { print $(NF-1) }' \
		| sort -u \
		| xargs -r dpkg-query --search \
		| cut -d: -f1 \
		| sort -u \
		| xargs -r apt-mark manual \
	; \
	apt-get purge -y --auto-remove -o APT::AutoRemove::RecommendsImportant=false; 
CMD ["redis-server", "--loadmodule", "/usr/lib/redis/modules/tdigest.so"]