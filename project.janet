(declare-project
  :name "jequests"
  :description "jequests"
  :author "Jelle Besseling"
  :license "GPLv3"
  :url "https://github.com/pingiun/jequests"
  :repo "https://github.com/pingiun/jequests"
  :dependencies ["https://github.com/janet-lang/json.git"])

(def lflags (case (os/which)
              :windows @["libcurl.lib"]
              :linux @["-lcurl"]
              #default
              @["-lcurl"]))

(declare-native
  :name "jequests"
  :lflags lflags
  :embedded ["jequests_lib.janet"]
  :source ["jequests.c"])
