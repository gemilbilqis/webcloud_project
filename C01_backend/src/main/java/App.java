import io.javalin.Javalin;
import io.javalin.http.UploadedFile;
import java.util.Base64;

import com.rabbitmq.client.Channel;
import com.rabbitmq.client.Connection;
import com.rabbitmq.client.ConnectionFactory;
import com.rabbitmq.client.GetResponse;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import com.rabbitmq.client.DeliverCallback;
import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;


public class App {
    public static void main(String[] args) {
        // BlockingQueue<String> resultQueue = new ArrayBlockingQueue<>(1);
        // new Thread(() -> {
        //     try {
        //         ConnectionFactory factory = new ConnectionFactory();
        //         factory.setHost("c02_rabbitmq");
        //         factory.setUsername("guest");
        //         factory.setPassword("guest");
        //         Connection connection = factory.newConnection();
        //         Channel channel = connection.createChannel();

        //         channel.queueDeclare("bmp_result", false, false, false, null);
        //         DeliverCallback deliverCallback = (consumerTag, delivery) -> {
        //             String message = new String(delivery.getBody(), StandardCharsets.UTF_8);
        //             System.out.println("Result received: ID = " + message);
        //             resultQueue.offer(message);
        //         };

        //         channel.basicConsume("bmp_result", true, deliverCallback, consumerTag -> {});
        //     } catch (Exception e) {
        //         e.printStackTrace();
        //     }
        // }).start();

        Javalin app = Javalin.create(config -> {
            config.plugins.enableDevLogging();
            config.staticFiles.add(staticFiles -> {
                staticFiles.hostedPath = "/";
                staticFiles.directory = "public";
                staticFiles.location = io.javalin.http.staticfiles.Location.EXTERNAL;
            });
        }).start(8080);

        app.post("/upload", ctx -> {
            UploadedFile image = ctx.uploadedFile("file");
            if (image == null) {
                System.out.println("Uploaded file is null!");
                ctx.status(400).result("No image received");
                return;
            }
            // Combine image + params
            byte[] imageBytes = image.content().readAllBytes();
            System.out.println("Image size: " + imageBytes.length + " bytes");

            String aesKey = ctx.formParam("key");
            String aesIV = ctx.formParam("iv");
            String mode = ctx.formParam("mode");
            String operation = ctx.formParam("operation");

            String header = aesKey + "|" + aesIV + "|" + mode + "|" + operation;
            byte[] headerBytes = header.getBytes();

            // Combine header + image
            byte[] message = new byte[headerBytes.length + imageBytes.length + 1];
            System.arraycopy(headerBytes, 0, message, 0, headerBytes.length);
            message[headerBytes.length] = (byte) 0x00; // delimiter
            System.arraycopy(imageBytes, 0, message, headerBytes.length + 1, imageBytes.length);

            System.out.println("Debug: Header length = " + headerBytes.length);
            System.out.println("Debug: Image length = " + imageBytes.length);
            System.out.println("Debug: Full message length = " + message.length);

            // Send to RabbitMQ
             try {
                ConnectionFactory factory = new ConnectionFactory();
                factory.setHost("c02_rabbitmq");
                factory.setUsername("guest");
                factory.setPassword("guest");

                try (Connection connection = factory.newConnection();
                     Channel channel = connection.createChannel()) {

                    // Send to bmp_encrypt queue
                    String encryptQueue = "bmp_encrypt";
                    channel.queueDeclare(encryptQueue, false, false, false, null);
                    channel.basicPublish("", encryptQueue, null, message);
                    System.out.println("Sent test message to queue: " + encryptQueue);

                    // Wait for response from bmp_result queue
                    String resultQueue = "bmp_result";
                    channel.queueDeclare(resultQueue, false, false, false, null);

                    String imageId = null;
                    for (int i = 0; i < 60; i++) { // wait up to 60 seconds
                        GetResponse response = channel.basicGet(resultQueue, true);
                        if (response != null) {
                            imageId = new String(response.getBody(), StandardCharsets.UTF_8);
                            System.out.println("📥 Result received: ID = " + imageId);
                            break;
                        }
                        Thread.sleep(1000);
                    }

                    if (imageId != null) {
                        String redirectHost = "localhost"; // or host.docker.internal if needed
                        int redirectPort = 3000;
                        String redirectUrl = String.format("http://%s:%d/images/%s", redirectHost, redirectPort, imageId);
                        System.out.println("🔁 Redirecting user to: " + redirectUrl);
                        ctx.redirect(redirectUrl);
                    } else {
                        ctx.result("Upload successful but no response received from processor.");
                    }
                }
            } catch (Exception e) {
                System.out.println("Failed to process message:");
                e.printStackTrace();
                ctx.status(500).result("Server error while processing upload.");
            }
        });
    }
}