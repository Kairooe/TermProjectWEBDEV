require('dotenv').config();
const express = require('express');
const cors = require('cors');
const connectDB = require('./src/config/db');

const app = express();

connectDB();

app.use(cors());
app.use(express.json());

app.use('/api/auth',   require('./src/routes/auth'));
app.use('/api/device', require('./src/routes/device'));
app.use('/api/study',  require('./src/routes/study'));

app.get('/health', (req, res) => res.json({ status: 'ok' }));

// Global error handler — catches any unhandled async errors from route handlers
app.use((err, req, res, _next) => {
  console.error('[server error]', err);
  res.status(500).json({ error: 'Internal server error' });
});

const PORT = process.env.PORT || 3001;
app.listen(PORT, () => console.log(`Server running on port ${PORT}`));
