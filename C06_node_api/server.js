require('dotenv').config();
const express = require('express');
const mysql = require('mysql2/promise');
const { MongoClient } = require('mongodb');
const app = express();
const port = process.env.PORT || 3000;

const dbConfig = {
  host: process.env.DB_HOST,
  user: process.env.DB_USER,
  password: process.env.DB_PASSWORD,
  database: process.env.DB_NAME,
};

const mongoUrl = 'mongodb://c07_mongo:27017';
const mongoDbName = 'snmp_db';

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

app.get('/metrics', async (req, res) => {
  try {
    const client = new MongoClient(mongoUrl);
    await client.connect();
    const db = client.db(mongoDbName);
    const collection = db.collection('metrics');

    const latestMetric = await collection
      .find({})
      .sort({ timestamp: -1 })
      .limit(1)
      .toArray();

    if (latestMetric.length === 0) {
      res.status(404).json({ message: 'No metrics found.' });
    } else {
      res.json(latestMetric[0]);
    }

    await client.close();
  } catch (err) {
    console.error(err);
    res.status(500).json({ message: 'Server error' });
  }
});

app.listen(port, () => {
  console.log(`BMP API listening on port ${port}`);
});
