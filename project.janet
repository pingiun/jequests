(declare-project
  :name "jequests"
  :description "jequests"
  :author "Jelle Besseling"
  :license "GPLv3"
  :url "https://github.com/pingiun/jequests"
  :repo "https://github.com/pingiun/jequests")

(def lflags (case (os/which)
              :windows @["libcurl.lib"]
              :linux @["-lcurl"]
              #default
              @["-lcurl"]))

(declare-native
  :name "cjequests"
  :lflags lflags
  :source ["jequests.c"])

(declare-source
  :source ["jequests.janet"])
