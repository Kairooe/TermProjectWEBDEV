const mongoose = require('mongoose');

const deviceSessionSchema = new mongoose.Schema({
  code: {
    type: String,
    required: true,
    unique: true,
  },
  userId: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'User',
    default: null,
  },
  userConfig: {
    type: Object,
    default: null,
  },
  status: {
    type: String,
    enum: ['pending', 'claimed'],
    default: 'pending',
  },
  expiresAt: {
    type: Date,
    required: true,
  },
  createdAt: {
    type: Date,
    default: Date.now,
  },
});

module.exports = mongoose.model('DeviceSession', deviceSessionSchema);
