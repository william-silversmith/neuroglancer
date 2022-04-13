# 2.0.24 (2021-06-10)
FROM emscripten/emsdk@sha256:81ec54b7a096d28f24d906955dbf98ff336cca47658d980c243baa36f6484f9f

ENV LIBJPEG_TURBO_2_1_3 dbda0c685942aa3ea908496592491e5ec8160d2cf1ec9d5fd5470e50768e7859

RUN mkdir -p /usr/src/libjpeg-turbo \
    && curl -SL -o /usr/src/libjpeg-turbo.tar.gz https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.3.tar.gz

RUN echo "${LIBJPEG_TURBO_2_1_3} /usr/src/libjpeg-turbo.tar.gz" | sha256sum --check --status
RUN tar -xzC /usr/src/libjpeg-turbo -f /usr/src/libjpeg-turbo.tar.gz --strip-components=1 \
    && rm /usr/src/libjpeg-turbo.tar.gz
