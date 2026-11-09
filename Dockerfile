FROM python:3.11-slim

WORKDIR /app

RUN apt-get update && apt-get install -y gcc && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

RUN gcc backend/main.c -o backend/filespace_server

EXPOSE 10000

CMD ["sh", "-c", "./backend/filespace_server & streamlit run app.py --server.address 0.0.0.0 --server.port ${PORT:-10000}"]