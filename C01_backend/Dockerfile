# First stage: Build
FROM maven:3.8.7-eclipse-temurin-17 AS build
WORKDIR /app
COPY pom.xml .
COPY src ./src
RUN mvn clean package

FROM eclipse-temurin:17-jdk-jammy AS base

WORKDIR /app

COPY --from=build /app/target/c01-backend-1.0-jar-with-dependencies.jar app.jar
COPY public /app/public

EXPOSE 8080

CMD ["java", "-jar", "app.jar"]
