FROM critoma/amd64_u24_noble_ism_security

WORKDIR /app

# Copy Java + C project files into container
COPY . /app

# Copy entrypoint to a safe path (outside /app to avoid overlay issues)
COPY entrypoint.sh /usr/local/bin/entrypoint.sh

# Make executable
RUN chmod +x /usr/local/bin/entrypoint.sh

# Install needed packages
# Install dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --fix-missing \
    openmpi-bin libopenmpi-dev openssh-server openssh-client sshpass \
    openjdk-17-jdk maven

# Make scripts executable
RUN chmod +x /app/wait-for-it.sh
RUN chmod +x /app/entrypoint.sh

# Build Java project
RUN mvn clean package && \
    cp target/c03-processor-1.0-jar-with-dependencies.jar /app/app.jar

# Compile C parallel encryption program
RUN mpicc -fopenmp encrypt_mpi.c -o encrypt_mpi -lssl -lcrypto

# Expose SSH
EXPOSE 22

# Launch script handles everything
ENTRYPOINT ["/app/entrypoint.sh"]
