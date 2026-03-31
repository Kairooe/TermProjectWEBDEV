import { createContext, useContext, useEffect, useState } from 'react';
import api from '../api/client';

const AuthContext = createContext(null);

export function AuthProvider({ children }) {
  const [user, setUser] = useState(null);
  const [token, setToken] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const storedToken = localStorage.getItem('token');
    const storedUsername = localStorage.getItem('username');
    if (!storedToken) { setLoading(false); return; }

    api.get('/api/auth/me')
      .then((res) => {
        setToken(storedToken);
        setUser({ username: res.data.username, email: res.data.email, studyConfig: res.data.studyConfig });
      })
      .catch(() => {
        localStorage.removeItem('token');
        localStorage.removeItem('username');
      })
      .finally(() => setLoading(false));
  }, []);

  const login = (newToken, username) => {
    localStorage.setItem('token', newToken);
    localStorage.setItem('username', username);
    setToken(newToken);
    setUser({ username });
  };

  const logout = () => {
    localStorage.removeItem('token');
    localStorage.removeItem('username');
    setToken(null);
    setUser(null);
  };

  return (
    <AuthContext.Provider value={{ user, token, login, logout, loading }}>
      {children}
    </AuthContext.Provider>
  );
}

export const useAuth = () => useContext(AuthContext);
