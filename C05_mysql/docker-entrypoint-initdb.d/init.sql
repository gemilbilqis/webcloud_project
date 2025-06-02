CREATE DATABASE IF NOT EXISTS webcloud;

USE webcloud;

CREATE TABLE IF NOT EXISTS encrypted_images (
    id INT AUTO_INCREMENT PRIMARY KEY,
    filename VARCHAR(255) NOT NULL,
    operation ENUM('encrypt', 'decrypt') NOT NULL,
    mode ENUM('ECB', 'CTR') NOT NULL,
    upload_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    image_data LONGBLOB NOT NULL
);
