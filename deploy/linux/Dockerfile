ARG PYTHON_IMAGE=python:3.12-slim
FROM ${PYTHON_IMAGE}

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_INDEX_URL=https://mirrors.cloud.tencent.com/pypi/simple \
    PIP_TRUSTED_HOST=mirrors.cloud.tencent.com

WORKDIR /app

COPY backend/requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY backend/app ./app
COPY deploy/video/viewer /app/viewer
COPY deploy/linux/docker-entrypoint.sh /usr/local/bin/qixiangzhan-entrypoint

RUN useradd --system --uid 10001 --create-home qixiangzhan \
    && chmod 0755 /usr/local/bin/qixiangzhan-entrypoint

EXPOSE 8000

ENTRYPOINT ["/usr/local/bin/qixiangzhan-entrypoint"]
