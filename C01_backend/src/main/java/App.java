import io.javalin.Javalin;
import io.javalin.http.UploadedFile;
import java.util.Base64;

import com.rabbitmq.client.Channel;
import com.rabbitmq.client.Connection;
import com.rabbitmq.client.ConnectionFactory;


public class App {
    public static void main(String[] args) {
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

            // DEBUG LOG
            System.out.println("Debug: First 30 bytes of message:");
            for (int i = 0; i < Math.min(30, message.length); i++) {
                System.out.printf("%02X ", message[i]);
            }
            System.out.println();

            System.out.println("Debug: Header length = " + headerBytes.length);
            System.out.println("Debug: Image length = " + imageBytes.length);
            System.out.println("Debug: Full message length = " + message.length);

            // Print area where 0x00 should be
            int checkStart = headerBytes.length - 2;
            int checkEnd = headerBytes.length + 3;
            System.out.print("Debug around delimiter: ");
            for (int i = checkStart; i < checkEnd; i++) {
                if (i >= 0 && i < message.length)
                    System.out.printf("[%02X]", message[i]);
            }
            System.out.println();


            // Send to RabbitMQ
            try {
                ConnectionFactory factory = new ConnectionFactory();
                factory.setHost("c02_rabbitmq");
                factory.setUsername("guest");
                factory.setPassword("guest");

                try (Connection connection = factory.newConnection();
                    Channel channel = connection.createChannel()) {

                    String queue = "bmp_encrypt";
                    channel.queueDeclare(queue, false, false, false, null);
                    System.out.println("Debug: First 20 bytes of message:");
                    for (int i = 0; i < Math.min(20, message.length); i++) {
                        System.out.printf("%02X ", message[i]);
                    }
                    System.out.println();
                    channel.basicPublish("", queue, null, message);

                    System.out.println("Sent test message to queue: " + queue);
                }
            } catch (Exception e) {
                System.out.println("Failed to publish to RabbitMQ:");
                e.printStackTrace(); // print full stack trace to logs
            }

            ctx.result("Upload received and attempted to publish.");
        });
    }
}
