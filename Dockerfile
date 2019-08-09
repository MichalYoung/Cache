# Docker build recipe for Cache
#  layered on srpc and adts images
#  (Work in progress July 2019, M Young)
#
FROM michalyoung/cache2019:srpc


ENV  LD_LIBRARY_PATH  /usr/local/lib

COPY . /usr/src/Cache
WORKDIR /usr/src/Cache
RUN   libtoolize &&\
      autoreconf -if &&\
      ./configure &&\
      make clean

# We let it keep an intermediate image at this point
# so that rebuilding from that image is short

RUN      make  &&\
      make install


