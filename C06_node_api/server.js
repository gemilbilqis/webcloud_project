require('dotenv').config();
const express = require('express');
const mysql = require('mysql2/promise');
const fs = require('fs');
const app = express();
const port = process.env.PORT || 3000;

const dbConfig = {
  host: process.env.DB_HOST,
  user: process.env.DB_USER,
  password: process.env.DB_PASSWORD,
  database: process.env.DB_NAME,
};

app.get('/images/:id', async (req, res) => {
  try {
    const conn = await mysql.createConnection(dbConfig);
    const [rows] = await conn.execute('SELECT filename, image_data FROM encrypted_images WHERE id = ?', [req.params.id]);
    if (rows.length === 0) {
      return res.status(404).send('Image not found');
    }
    const img = rows[0];
    res.set('Content-Disposition', `attachment; filename="${img.filename}"`);
    res.set('Content-Type', 'image/bmp');
    res.send(img.image_data);
    await conn.end();
  } catch (err) {
    console.error(err);
    res.status(500).send('Server error');
  }
});

app.listen(port, () => {
  console.log(`BMP API listening on port ${port}`);
});
