const mongoose = require('mongoose');

const studyRecordSchema = new mongoose.Schema({
  userId: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'User',
    required: true,
  },
  sessionId: { type: String },
  question: { type: String },
  options: { type: Object },
  selectedAnswer: { type: String },
  correctAnswer: { type: String },
  isCorrect: { type: Boolean },
  subject: { type: String },
  questionNumber: { type: Number },
  timestamp: { type: Date, default: Date.now },
});

module.exports = mongoose.model('StudyRecord', studyRecordSchema);
