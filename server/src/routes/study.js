const express = require('express');
const mongoose = require('mongoose');
const StudyRecord = require('../models/StudyRecord');
const User = require('../models/User');
const { verifyToken } = require('../middleware/auth');

const router = express.Router();
router.use(verifyToken);

// POST /api/study/record
router.post('/record', async (req, res) => {
  try {
    const userId = req.user.userId;
    const raw = Array.isArray(req.body) ? req.body : [req.body];

    const docs = raw.map(({ sessionId, question, options, selectedAnswer, correctAnswer, isCorrect, subject, questionNumber, timestamp }) => ({
      userId,
      sessionId,
      question,
      options,
      selectedAnswer,
      correctAnswer,
      isCorrect,
      subject,
      questionNumber,
      timestamp,
    }));

    const result = await StudyRecord.insertMany(docs);
    res.status(201).json({ saved: result.length });
  } catch (err) {
    console.error('[study/record]', err);
    res.status(500).json({ error: 'Failed to save record' });
  }
});

// GET /api/study/history
router.get('/history', async (req, res) => {
  try {
    const userId = req.user.userId;
    const limit = Math.min(parseInt(req.query.limit) || 50, 200);
    const skip = parseInt(req.query.skip) || 0;

    const filter = { userId };
    if (req.query.subject) filter.subject = req.query.subject;

    const records = await StudyRecord.find(filter)
      .sort({ timestamp: -1 })
      .skip(skip)
      .limit(limit)
      .lean();

    res.json(records);
  } catch (err) {
    console.error('[study/history]', err);
    res.status(500).json({ error: 'Failed to fetch history' });
  }
});

// GET /api/study/stats
router.get('/stats', async (req, res) => {
  try {
    const userId = new mongoose.Types.ObjectId(req.user.userId);

    const [overall] = await StudyRecord.aggregate([
      { $match: { userId } },
      {
        $group: {
          _id: null,
          totalQuestions: { $sum: 1 },
          totalCorrect: { $sum: { $cond: ['$isCorrect', 1, 0] } },
        },
      },
      {
        $project: {
          _id: 0,
          totalQuestions: 1,
          totalCorrect: 1,
          accuracy: {
            $round: [
              { $multiply: [{ $divide: ['$totalCorrect', '$totalQuestions'] }, 100] },
              1,
            ],
          },
        },
      },
    ]);

    const breakdown = await StudyRecord.aggregate([
      { $match: { userId } },
      {
        $group: {
          _id: '$subject',
          total: { $sum: 1 },
          correct: { $sum: { $cond: ['$isCorrect', 1, 0] } },
        },
      },
      {
        $project: {
          _id: 0,
          subject: '$_id',
          total: 1,
          correct: 1,
          accuracy: {
            $round: [
              { $multiply: [{ $divide: ['$correct', '$total'] }, 100] },
              1,
            ],
          },
        },
      },
      { $sort: { subject: 1 } },
    ]);

    const recent = await StudyRecord.find({ userId })
      .sort({ timestamp: -1 })
      .select('isCorrect')
      .lean();

    let streak = 0;
    for (const r of recent) {
      if (r.isCorrect) streak++;
      else break;
    }

    res.json({
      ...(overall ?? { totalQuestions: 0, totalCorrect: 0, accuracy: 0 }),
      breakdown,
      streak,
    });
  } catch (err) {
    console.error('[study/stats]', err);
    res.status(500).json({ error: 'Failed to fetch stats' });
  }
});

// PUT /api/study/config
router.put('/config', async (req, res) => {
  try {
    const { subject, difficulty, customNotes } = req.body;

    const user = await User.findByIdAndUpdate(
      req.user.userId,
      { $set: { studyConfig: { subject, difficulty, customNotes } } },
      { new: true }
    ).select('studyConfig');

    if (!user) return res.status(404).json({ error: 'User not found' });
    res.json(user.studyConfig);
  } catch (err) {
    console.error('[study/config]', err);
    res.status(500).json({ error: 'Failed to save config' });
  }
});

// GET /api/study/leaderboard
router.get('/leaderboard', async (req, res) => {
  try {
    const top = await StudyRecord.aggregate([
      {
        $group: {
          _id: '$userId',
          totalQuestions: { $sum: 1 },
          totalCorrect: { $sum: { $cond: ['$isCorrect', 1, 0] } },
        },
      },
      { $match: { totalQuestions: { $gte: 10 } } },
      {
        $project: {
          _id: 0,
          userId: '$_id',
          totalQuestions: 1,
          accuracy: {
            $round: [
              { $multiply: [{ $divide: ['$totalCorrect', '$totalQuestions'] }, 100] },
              1,
            ],
          },
        },
      },
      { $sort: { accuracy: -1 } },
      { $limit: 10 },
      {
        $lookup: {
          from: 'users',
          localField: 'userId',
          foreignField: '_id',
          as: 'user',
        },
      },
      { $unwind: '$user' },
      {
        $project: {
          username: '$user.username',
          totalQuestions: 1,
          accuracy: 1,
        },
      },
    ]);

    res.json(top);
  } catch (err) {
    console.error('[study/leaderboard]', err);
    res.status(500).json({ error: 'Failed to fetch leaderboard' });
  }
});

module.exports = router;
