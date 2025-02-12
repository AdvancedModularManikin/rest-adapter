-- Module capabilities table
CREATE TABLE IF NOT EXISTS module_capabilities (
                                                   module_id TEXT PRIMARY KEY,
                                                   module_guid TEXT UNIQUE,
                                                   module_name TEXT NOT NULL,
                                                   description TEXT,
                                                   capabilities TEXT,
                                                   manufacturer TEXT,
                                                   model TEXT,
                                                   created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                                                   updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Events table
CREATE TABLE IF NOT EXISTS events (
                                      id INTEGER PRIMARY KEY AUTOINCREMENT,
                                      source TEXT,
                                      topic TEXT,
                                      timestamp INTEGER,
                                      tick INTEGER,
                                      data TEXT,
                                      created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Logs table
CREATE TABLE IF NOT EXISTS logs (
                                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                                    module_name TEXT,
                                    module_guid TEXT,
                                    module_id TEXT,
                                    message TEXT,
                                    log_level TEXT,
                                    timestamp INTEGER,
                                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);
CREATE INDEX IF NOT EXISTS idx_events_source ON events(source);
CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp);
CREATE INDEX IF NOT EXISTS idx_logs_module ON logs(module_name);
CREATE INDEX IF NOT EXISTS idx_capabilities_name ON module_capabilities(module_name);

-- Triggers
CREATE TRIGGER IF NOT EXISTS update_module_capabilities_timestamp
    AFTER UPDATE ON module_capabilities
BEGIN
    UPDATE module_capabilities
    SET updated_at = CURRENT_TIMESTAMP
    WHERE module_id = NEW.module_id;
END;