FROM debian:jessie

RUN apt-get update && apt-get install -y --no-install-recommends \
        libxapian-dev libgmime-2.6-dev libgcrypt20-dev
	&& rm -rf /var/lib/apt/lists/*
