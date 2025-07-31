#pragma once

class AppConfig {
  public:
    static AppConfig& instance() {
        static AppConfig instance;
        return instance;
    }

    void setQueueSize(int size) { m_queueSize = size; }
    int getQueueSize() const { return m_queueSize; }

  private:
    AppConfig() = default;
    int m_queueSize = 50; // Default queue size
};
