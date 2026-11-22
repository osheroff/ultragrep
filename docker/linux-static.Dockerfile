# Builds fully static (musl) linux ug_guts and ug_cat binaries into /out.
FROM alpine:3.20
RUN apk add --no-cache build-base pcre-dev lua5.2-dev zlib-dev zlib-static pkgconf
WORKDIR /src
COPY src/ ./
RUN set -e; \
    CFLAGS="-Wall -O3 -static $(pkg-config --cflags lua5.2)"; \
    LIBS="$(pkg-config --static --libs lua5.2) -lpcre"; \
    mkdir /out; \
    gcc $CFLAGS -o /out/ug_guts ug_guts.c ug_lua.c $LIBS; \
    gcc $CFLAGS -o /out/ug_cat ug_cat.c ug_index.c ug_build_index.c ug_gzip.c ug_lua.c -lz $LIBS; \
    strip /out/ug_guts /out/ug_cat
