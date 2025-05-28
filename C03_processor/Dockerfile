FROM critoma/amd64_u24_noble_ism_security

WORKDIR /app

# Copy all sources (Java + C)
COPY . .

# Install needed packages
RUN apt update && apt install -y \
    openmpi-bin libopenmpi-dev openssh-server openssh-client sshpass openjdk-17-jdk maven

# Make scripts executable
RUN chmod +x /app/wait-for-it.sh
RUN chmod +x /app/entrypoint.sh

# Build Java project
RUN mvn clean package && cp target/c03-processor-1.0-jar-with-dependencies.jar /app/app.jar

# Compile C encryption program
RUN mpicc -fopenmp encrypt_mpi.c -o encrypt_mpi -lssl -lcrypto

# SSH port exposed
EXPOSE 22

# Entrypoint handles SSH, RabbitMQ wait, and starts app
ENTRYPOINT ["/app/entrypoint.sh"]
