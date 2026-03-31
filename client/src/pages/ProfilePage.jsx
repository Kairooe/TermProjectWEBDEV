import { useEffect, useState } from 'react';
import NavBar from '../components/NavBar';
import api from '../api/client';

const DIFFICULTIES = ['easy', 'medium', 'hard'];

export default function ProfilePage() {
  const [config, setConfig] = useState({ subject: '', difficulty: 'easy', customNotes: '' });
  const [history, setHistory] = useState([]);
  const [saveMsg, setSaveMsg] = useState('');
  const [saveError, setSaveError] = useState('');
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    api.get('/api/auth/me').then((res) => {
      if (res.data.studyConfig) {
        setConfig({
          subject: res.data.studyConfig.subject || '',
          difficulty: res.data.studyConfig.difficulty || 'easy',
          customNotes: res.data.studyConfig.customNotes || '',
        });
      }
    });
    api.get('/api/study/history?limit=20').then((res) => setHistory(res.data));
  }, []);

  const handleSave = async () => {
    setSaveMsg('');
    setSaveError('');
    setSaving(true);
    try {
      const { data } = await api.put('/api/study/config', config);
      setConfig({ subject: data.subject || '', difficulty: data.difficulty || 'easy', customNotes: data.customNotes || '' });
      setSaveMsg('Config saved!');
    } catch {
      setSaveError('Failed to save config.');
    } finally {
      setSaving(false);
    }
  };

  return (
    <div style={s.page}>
      <NavBar />
      <div style={s.content}>
        <h1 style={s.heading}>Profile</h1>

        {/* Study config */}
        <div style={s.section}>
          <h2 style={s.sectionTitle}>Study Configuration</h2>

          <div style={s.field}>
            <label style={s.label}>Subject</label>
            <input
              style={s.input}
              type="text"
              placeholder="e.g. Biology, History…"
              value={config.subject}
              onChange={(e) => setConfig({ ...config, subject: e.target.value })}
            />
          </div>

          <div style={s.field}>
            <label style={s.label}>Difficulty</label>
            <select
              style={s.select}
              value={config.difficulty}
              onChange={(e) => setConfig({ ...config, difficulty: e.target.value })}
            >
              {DIFFICULTIES.map((d) => (
                <option key={d} value={d}>{d.charAt(0).toUpperCase() + d.slice(1)}</option>
              ))}
            </select>
          </div>

          <div style={s.field}>
            <label style={s.label}>Custom Notes</label>
            <textarea
              style={s.textarea}
              rows={6}
              placeholder="Paste lecture notes here — the AI will generate questions from this content."
              value={config.customNotes}
              onChange={(e) => setConfig({ ...config, customNotes: e.target.value })}
            />
          </div>

          <div style={{ ...s.saveBtn, opacity: saving ? 0.7 : 1 }} onClick={!saving ? handleSave : undefined}>
            {saving ? 'Saving…' : 'Save Config'}
          </div>
          {saveMsg && <p style={s.success}>{saveMsg}</p>}
          {saveError && <p style={s.error}>{saveError}</p>}
        </div>

        {/* Recent history */}
        <div style={s.section}>
          <h2 style={s.sectionTitle}>Recent History</h2>
          {history.length === 0
            ? <p style={s.empty}>No questions answered yet.</p>
            : (
              <div style={s.tableWrap}>
                <table style={s.table}>
                  <thead>
                    <tr>
                      {['Question', 'Your Answer', 'Correct', '✓/✗', 'Time'].map((h) => (
                        <th key={h} style={s.th}>{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {history.map((r, i) => (
                      <tr key={r._id ?? i} style={{ background: i % 2 === 0 ? '#fff' : '#fafafa' }}>
                        <td style={{ ...s.td, maxWidth: 260, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{r.question || '—'}</td>
                        <td style={s.td}>{r.selectedAnswer || '—'}</td>
                        <td style={s.td}>{r.correctAnswer || '—'}</td>
                        <td style={{ ...s.td, textAlign: 'center', fontSize: 16 }}>{r.isCorrect ? '✅' : '❌'}</td>
                        <td style={{ ...s.td, whiteSpace: 'nowrap', color: '#999', fontSize: 12 }}>
                          {r.timestamp ? new Date(r.timestamp).toLocaleString() : '—'}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
        </div>
      </div>
    </div>
  );
}

const s = {
  page: { minHeight: '100vh', background: '#f0f2f5' },
  content: { maxWidth: 860, margin: '0 auto', padding: '36px 24px' },
  heading: { fontSize: 26, fontWeight: 700, color: '#1a1a2e', marginBottom: 28 },
  section: { background: '#fff', borderRadius: 12, padding: '28px 24px', boxShadow: '0 1px 8px rgba(0,0,0,0.08)', marginBottom: 24 },
  sectionTitle: { fontSize: 18, fontWeight: 700, color: '#1a1a2e', margin: '0 0 20px' },
  field: { marginBottom: 18 },
  label: { display: 'block', marginBottom: 6, fontSize: 13, fontWeight: 600, color: '#444' },
  input: { width: '100%', padding: '10px 12px', border: '1.5px solid #ddd', borderRadius: 8, fontSize: 15, outline: 'none', boxSizing: 'border-box' },
  select: { width: '100%', padding: '10px 12px', border: '1.5px solid #ddd', borderRadius: 8, fontSize: 15, outline: 'none', background: '#fff', color: '#333', boxSizing: 'border-box' },
  textarea: { width: '100%', padding: '10px 12px', border: '1.5px solid #ddd', borderRadius: 8, fontSize: 14, outline: 'none', resize: 'vertical', boxSizing: 'border-box', fontFamily: 'inherit' },
  saveBtn: { display: 'inline-block', padding: '10px 28px', background: '#4f46e5', color: '#fff', borderRadius: 8, fontWeight: 600, fontSize: 15, cursor: 'pointer', userSelect: 'none' },
  success: { marginTop: 10, color: '#27ae60', fontWeight: 500, fontSize: 14 },
  error: { marginTop: 10, color: '#c0392b', fontSize: 14 },
  empty: { color: '#aaa', fontSize: 14 },
  tableWrap: { overflowX: 'auto' },
  table: { width: '100%', borderCollapse: 'collapse' },
  th: { textAlign: 'left', padding: '8px 12px', fontSize: 13, fontWeight: 600, color: '#888', borderBottom: '2px solid #eee', whiteSpace: 'nowrap' },
  td: { padding: '10px 12px', fontSize: 14, color: '#333', borderBottom: '1px solid #f0f0f0' },
};
