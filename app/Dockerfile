FROM ghcr.io/cirruslabs/flutter:stable

# Accept Android SDK licenses
RUN yes | flutter doctor --android-licenses 2>/dev/null || true

# Install required Android SDK components
RUN sdkmanager --install \
    "platforms;android-35" \
    "build-tools;35.0.0" \
    "platform-tools"

WORKDIR /app

# Cache pub dependencies
COPY pubspec.yaml pubspec.lock* ./
RUN flutter pub get || true

# Copy source
COPY . .
RUN flutter pub get

CMD ["flutter", "build", "apk", "--debug"]
