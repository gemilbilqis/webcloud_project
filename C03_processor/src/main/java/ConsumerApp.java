import com.rabbitmq.client.*;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class ConsumerApp {
    private static final String QUEUE_NAME = "bmp_encrypt";

    public static void main(String[] args) throws Exception {
        ConnectionFactory factory = new ConnectionFactory();
        factory.setHost("c02_rabbitmq");
        factory.setUsername("guest");
        factory.setPassword("guest");

        Connection connection = factory.newConnection();
        Channel channel = connection.createChannel();

        System.out.println("Connected to RabbitMQ on 172.18.0.3");
        System.out.println("Channel open");

        channel.queueDeclare(QUEUE_NAME, false, false, false, null);
        System.out.println("Waiting for messages...");

        DeliverCallback callback = (consumerTag, delivery) -> {
            System.out.println("Message received in consumer");
            byte[] body = delivery.getBody();

            int delimiterIndex = -1;
            for (int i = 0; i < body.length; i++) {
                if (body[i] == 0x00) {
                    delimiterIndex = i;
                    break;
                }
            }

            if (delimiterIndex == -1) {
                System.err.println("Invalid message format: no delimiter");
                return;
            }

            String header = new String(body, 0, delimiterIndex, StandardCharsets.UTF_8);
            byte[] imageBytes = new byte[body.length - delimiterIndex - 1];
            System.arraycopy(body, delimiterIndex + 1, imageBytes, 0, imageBytes.length);

            String[] parts = header.split("\\|");
            String key = parts[0];
            String iv = parts[1];
            String mode = parts[2];
            String operation = parts[3];

            try (FileOutputStream fos = new FileOutputStream("/tmp/input.bmp")) {
                fos.write(imageBytes);
            }

            System.out.println("Received image + AES params");
            System.out.println("Launching encryption: mode=" + mode + ", op=" + operation);

            String[] cmd = {
                "mpirun", "--allow-run-as-root", "-n", "2",
                "/app/encrypt_mpi",
                "/tmp/input.bmp",
                "/tmp/output.bmp",
                mode,
                key,
                iv
            };

            try {
                ProcessBuilder pb = new ProcessBuilder(cmd);
                pb.redirectErrorStream(true);
                Process proc = pb.start();

                new Thread(() -> {
                    try (var reader = new java.io.BufferedReader(
                            new java.io.InputStreamReader(proc.getInputStream()))) {
                        String line;
                        while ((line = reader.readLine()) != null) {
                            System.out.println("[encrypt_mpi] " + line);
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }).start();

                int exitCode = proc.waitFor();
                java.io.File outFile = new java.io.File("/tmp/output.bmp");
                if (outFile.exists()) {
                    System.out.println("Output BMP is ready at /tmp/output.bmp");
                } else {
                    System.err.println("Output BMP not found!");
                }

                System.out.println("Encryption process finished with exit code: " + exitCode);
            } catch (Exception e) {
                System.err.println("Failed to launch encryption");
                e.printStackTrace();
            }

            // Manual message acknowledgment
            channel.basicAck(delivery.getEnvelope().getDeliveryTag(), false);
        };

        // ❗ Use manual ack mode (false)
        channel.basicConsume(QUEUE_NAME, false, callback, consumerTag -> { });

        // Keep the app running
        System.out.println("Consumer is running.");
        Thread.currentThread().join();
    }
}
