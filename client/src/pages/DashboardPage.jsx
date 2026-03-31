import { useEffect, useState } from 'react';
import { useAuth } from '../context/AuthContext';
import NavBar from '../components/NavBar';
import api from '../api/client';

export default function DashboardPage() {
  const { user } = useAuth();
  const [stats, setStats] = useState(null);
  const [statsError, setStatsError] = useState('');
  const [code, setCode] = useState('');
  const [claimMsg, setClaimMsg] = useState('');
  const [claimError, setClaimError] = useState('');
  const [claiming, setClaiming] = useState(false);

  useEffect(() => {
    api.get('/api/study/stats')
      .then((res) => setStats(res.data))
      .catch(() => setStatsError('Could not load stats.'));
  }, []);

  const handleClaim = async () => {
    setClaimMsg('');
    setClaimError('');
    if (!/^\d{6}$/.test(code)) { setClaimError('Enter a valid 6-digit code.'); return; }
    setClaiming(true);
    try {
      const { data } = await api.post('/api/device/claim', { code });
      setClaimMsg(`ESP32 connected! Logged in as ${data.username}`);
      setCode('');
    } catch {
      setClaimError('Invalid or expired code.');
    } finally {
      setClaiming(false);
    }
  };

  return (
    <div style={s.page}>
      <NavBar />
      <div style={s.content}>
        <h1 style={s.heading}>Welcome back, {user?.username}</h1>

        {/* Stats cards */}
        {statsError && <p style={s.error}>{statsError}</p>}
        {stats && (
          <div style={s.cards}>
            <StatCard label="Total Questions" value={stats.totalQuestions} />
            <StatCard label="Accuracy" value={`${stats.accuracy ?? 0}%`} />
            <StatCard label="Current Streak" value={stats.streak} />
          </div>
        )}

        {/* Subject breakdown */}
        {stats?.breakdown?.length > 0 && (
          <div style={s.section}>
            <h2 style={s.sectionTitle}>Subject Breakdown</h2>
            <table style={s.table}>
              <thead>
                <tr>
                  {['Subject', 'Total', 'Correct', 'Accuracy'].map((h) => (
                    <th key={h} style={s.th}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {stats.breakdown.map((row) => (
                  <tr key={row.subject}>
                    <td style={s.td}>{row.subject || '—'}</td>
                    <td style={s.td}>{row.total}</td>
                    <td style={s.td}>{row.correct}</td>
                    <td style={s.td}>{row.accuracy}%</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}

        {/* ESP32 pairing */}
        <div style={s.section}>
          <h2 style={s.sectionTitle}>Connect ESP32</h2>
          <p style={s.hint}>Enter the 6-digit code shown on your ESP32 display.</p>
          <div style={s.row}>
            <input
              style={s.codeInput}
              type="text"
              maxLength={6}
              value={code}
              placeholder="123456"
              onChange={(e) => setCode(e.target.value.replace(/\D/g, ''))}
              onKeyDown={(e) => e.key === 'Enter' && handleClaim()}
            />
            <div style={{ ...s.claimBtn, opacity: claiming ? 0.7 : 1 }} onClick={!claiming ? handleClaim : undefined}>
              {claiming ? 'Connecting…' : 'Connect'}
            </div>
          </div>
          {claimMsg && <p style={s.success}>{claimMsg}</p>}
          {claimError && <p style={s.error}>{claimError}</p>}
        </div>
      </div>
    </div>
  );
}

function StatCard({ label, value }) {
  return (
    <div style={s.card}>
      <div style={s.cardValue}>{value ?? '—'}</div>
      <div style={s.cardLabel}>{label}</div>
    </div>
  );
}

const s = {
  page: { minHeight: '100vh', background: '#f0f2f5' },
  content: { maxWidth: 860, margin: '0 auto', padding: '36px 24px' },
  heading: { fontSize: 26, fontWeight: 700, color: '#1a1a2e', marginBottom: 28 },
  cards: { display: 'flex', gap: 20, marginBottom: 36, flexWrap: 'wrap' },
  card: { flex: '1 1 160px', background: '#fff', borderRadius: 12, padding: '24px 20px', boxShadow: '0 1px 8px rgba(0,0,0,0.08)', textAlign: 'center' },
  cardValue: { fontSize: 32, fontWeight: 700, color: '#4f46e5', marginBottom: 6 },
  cardLabel: { fontSize: 13, color: '#666', fontWeight: 500 },
  section: { background: '#fff', borderRadius: 12, padding: '28px 24px', boxShadow: '0 1px 8px rgba(0,0,0,0.08)', marginBottom: 24 },
  sectionTitle: { fontSize: 18, fontWeight: 700, color: '#1a1a2e', margin: '0 0 16px' },
  table: { width: '100%', borderCollapse: 'collapse' },
  th: { textAlign: 'left', padding: '8px 12px', fontSize: 13, fontWeight: 600, color: '#888', borderBottom: '1px solid #eee' },
  td: { padding: '10px 12px', fontSize: 14, color: '#333', borderBottom: '1px solid #f5f5f5' },
  hint: { color: '#888', fontSize: 14, margin: '0 0 14px' },
  row: { display: 'flex', gap: 12, alignItems: 'center' },
  codeInput: { padding: '10px 14px', border: '1.5px solid #ddd', borderRadius: 8, fontSize: 20, letterSpacing: 6, width: 140, textAlign: 'center', outline: 'none' },
  claimBtn: { padding: '10px 24px', background: '#4f46e5', color: '#fff', borderRadius: 8, fontWeight: 600, fontSize: 15, cursor: 'pointer', userSelect: 'none' },
  success: { marginTop: 12, color: '#27ae60', fontWeight: 500, fontSize: 14 },
  error: { marginTop: 12, color: '#c0392b', fontSize: 14 },
};
