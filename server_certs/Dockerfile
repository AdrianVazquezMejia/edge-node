#
# Dockerfile
#
FROM ubuntu:18.04

RUN apt-get update
RUN apt-get install -y openssl
RUN apt-get install -y unzip
RUN apt-get install -y wget
WORKDIR /server
COPY ca_cert.pem .
COPY ca_key.pem .
COPY update.sh update.sh
EXPOSE 8070
CMD ["openssl", "s_server", "-WWW", "-key","ca_key.pem", "-cert","ca_cert.pem" ,"-port" ,"8070"]
