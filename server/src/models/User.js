const mongoose = require('mongoose');

const userSchema = new mongoose.Schema({
  username: {
    type: String,
    required: true,
    unique: true,
    trim: true,
  },
  email: {
    type: String,
    required: true,
    unique: true,
    lowercase: true,
    trim: true,
  },
  passwordHash: {
    type: String,
    required: true,
  },
  createdAt: {
    type: Date,
    default: Date.now,
  },
  studyConfig: {
    subject: { type: String },
    difficulty: { type: String, enum: ['easy', 'medium', 'hard'] },
    customNotes: { type: String },
  },
});

module.exports = mongoose.model('User', userSchema);
