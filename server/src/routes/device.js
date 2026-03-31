const express = require('express');
const DeviceSession = require('../models/DeviceSession');
const StudyRecord = require('../models/StudyRecord');
const User = require('../models/User');
const { verifyToken } = require('../middleware/auth');

const router = express.Router();

// POST /api/device/register — called by ESP32 on boot
router.post('/register', async (req, res) => {
  try {
    const { code } = req.body;

    if (!code || !/^\d{6}$/.test(code))
      return res.status(400).json({ error: 'code must be a 6-digit numeric string' });

    await DeviceSession.deleteOne({ code, status: 'pending' });

    const expiresAt = new Date(Date.now() + 5 * 60 * 1000);
    const session = await DeviceSession.create({ code, expiresAt });

    res.status(201).json({ code: session.code, expiresAt: session.expiresAt });
  } catch (err) {
    console.error('[device/register]', err);
    res.status(500).json({ error: 'Failed to register device' });
  }
});

// GET /api/device/poll/:code — polled by ESP32 every 3s
router.get('/poll/:code', async (req, res) => {
  try {
    const session = await DeviceSession.findOne({ code: req.params.code });

    if (!session || session.expiresAt < new Date())
      return res.status(404).json({ error: 'Session not found or expired' });

    if (session.status === 'claimed') {
      const user = await User.findById(session.userId).select('username');
      return res.json({
        claimed: true,
        userConfig: session.userConfig,
        username: user ? user.username : 'Player',
      });
    }

    res.json({ claimed: false });
  } catch (err) {
    console.error('[device/poll]', err);
    res.status(500).json({ error: 'Poll failed' });
  }
});

// POST /api/device/claim — called by authenticated web user
router.post('/claim', verifyToken, async (req, res) => {
  try {
    const { code } = req.body;

    if (!code) return res.status(400).json({ error: 'code is required' });

    const session = await DeviceSession.findOne({
      code,
      status: 'pending',
      expiresAt: { $gt: new Date() },
    });

    if (!session) return res.status(404).json({ error: 'Session not found, expired, or already claimed' });

    const user = await User.findById(req.user.userId).select('username studyConfig');
    if (!user) return res.status(404).json({ error: 'User not found' });

    session.userId = req.user.userId;
    session.status = 'claimed';
    session.userConfig = user.studyConfig ?? null;
    await session.save();

    res.json({ success: true, username: user.username });
  } catch (err) {
    console.error('[device/claim]', err);
    res.status(500).json({ error: 'Claim failed' });
  }
});

// POST /api/device/record — called by ESP32 after each answer (no JWT needed)
// Authenticates using the device code; saves the record for the claimed session's user.
router.post('/record', async (req, res) => {
  try {
    const { code, question, selectedAnswer, correctAnswer, isCorrect, subject, questionNumber, timestamp } = req.body;

    if (!code) return res.status(400).json({ error: 'code is required' });

    const session = await DeviceSession.findOne({ code, status: 'claimed' });
    if (!session) return res.status(404).json({ error: 'No claimed session for this code' });

    await StudyRecord.create({
      userId: session.userId,
      question,
      selectedAnswer,
      correctAnswer,
      isCorrect,
      subject: subject || 'general',
      questionNumber,
      timestamp: timestamp || new Date(),
    });

    res.status(201).json({ saved: true });
  } catch (err) {
    console.error('[device/record]', err);
    res.status(500).json({ error: 'Failed to save record' });
  }
});

module.exports = router;
