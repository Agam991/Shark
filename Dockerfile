
FROM node:20-bookworm

# Install Linux build tools, Python, and Stockfish
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    g++ \
    python3 \
    python3-venv \
    stockfish \
    && rm -rf /var/lib/apt/lists/* \
    && ln -sf /usr/games/stockfish /usr/local/bin/stockfish

WORKDIR /app

# Copy the Shark repository
COPY . .

# Build Shark for Render's Linux environment
RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DSHARK_NATIVE=OFF \
    && cmake --build build -j2

# Install Python dependencies in a virtual environment
RUN python3 -m venv /app/.venv \
    && /app/.venv/bin/pip install --no-cache-dir \
       -r tools/requirements.txt

# Install the web server dependencies
RUN npm install --prefix web

ENV PATH="/app/.venv/bin:${PATH}"

EXPOSE 3000

CMD ["npm", "start", "--prefix", "web"]
