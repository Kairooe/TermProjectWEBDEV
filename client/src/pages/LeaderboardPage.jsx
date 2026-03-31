import { useEffect, useState } from 'react';
import { useAuth } from '../context/AuthContext';
import NavBar from '../components/NavBar';
import api from '../api/client';

export default function LeaderboardPage() {
  const { user } = useAuth();
  const [rows, setRows] = useState([]);
  const [error, setError] = useState('');

  useEffect(() => {
    api.get('/api/study/leaderboard')
      .then((res) => setRows(res.data))
      .catch(() => setError('Could not load leaderboard.'));
  }, []);

  return (
    <div style={s.page}>
      <NavBar />
      <div style={s.content}>
        <h1 style={s.heading}>Leaderboard</h1>
        <p style={s.hint}>Top 10 players with at least 10 questions answered.</p>

        {error && <p style={s.error}>{error}</p>}

        {rows.length === 0 && !error && (
          <p style={s.empty}>No entries yet — answer some questions to appear here!</p>
        )}

        {rows.length > 0 && (
          <div style={s.section}>
            <table style={s.table}>
              <thead>
                <tr>
                  {['#', 'Username', 'Accuracy', 'Total Questions'].map((h) => (
                    <th key={h} style={s.th}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {rows.map((row, i) => {
                  const isMe = row.username === user?.username;
                  return (
                    <tr key={row.username} style={{ background: isMe ? '#eef2ff' : i % 2 === 0 ? '#fff' : '#fafafa' }}>
                      <td style={s.td}>
                        {i === 0 ? '🥇' : i === 1 ? '🥈' : i === 2 ? '🥉' : i + 1}
                      </td>
                      <td style={{ ...s.td, fontWeight: isMe ? 700 : 400, color: isMe ? '#4f46e5' : '#333' }}>
                        {row.username}{isMe ? ' (you)' : ''}
                      </td>
                      <td style={s.td}>{row.accuracy}%</td>
                      <td style={s.td}>{row.totalQuestions}</td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
}

const s = {
  page: { minHeight: '100vh', background: '#f0f2f5' },
  content: { maxWidth: 700, margin: '0 auto', padding: '36px 24px' },
  heading: { fontSize: 26, fontWeight: 700, color: '#1a1a2e', marginBottom: 6 },
  hint: { color: '#888', fontSize: 14, marginBottom: 28 },
  section: { background: '#fff', borderRadius: 12, padding: '8px 0', boxShadow: '0 1px 8px rgba(0,0,0,0.08)' },
  table: { width: '100%', borderCollapse: 'collapse' },
  th: { textAlign: 'left', padding: '12px 20px', fontSize: 13, fontWeight: 600, color: '#888', borderBottom: '2px solid #eee' },
  td: { padding: '14px 20px', fontSize: 15, color: '#333', borderBottom: '1px solid #f0f0f0' },
  error: { color: '#c0392b', fontSize: 14 },
  empty: { color: '#aaa', fontSize: 14 },
};
