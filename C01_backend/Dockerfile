FROM eclipse-temurin:17-jdk-jammy AS base

WORKDIR /app

COPY target/c01-backend-1.0-jar-with-dependencies.jar app.jar
COPY public /app/public

EXPOSE 8080

CMD ["java", "-jar", "app.jar"]
