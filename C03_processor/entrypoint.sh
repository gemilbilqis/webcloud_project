#!/bin/bash

# Start SSH service
service ssh start

# Generate SSH key if it doesn't exist
if [ ! -f /root/.ssh/id_rsa ]; then
  ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa
fi

# Wait until C04 is accessible via SSH
until sshpass -p "root" ssh -o StrictHostKeyChecking=no root@c04_worker "echo Connected"; do
  echo "Waiting for SSH to c04_worker..."
  sleep 2
done

# Set up authorized_keys on C04
sshpass -p "root" ssh -o StrictHostKeyChecking=no root@c04_worker "mkdir -p /root/.ssh && touch /root/.ssh/authorized_keys"
sshpass -p "root" ssh -o StrictHostKeyChecking=no root@c04_worker "echo \"$(cat /root/.ssh/id_rsa.pub)\" >> /root/.ssh/authorized_keys"

# Wait for RabbitMQ to be ready
./wait-for-it.sh c02_rabbitmq:5672 --timeout=60 --strict -- echo "RabbitMQ is ready"

# Launch the Java consumer
exec java -jar /app/app.jar
