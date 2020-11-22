(import json)

(def- tableget get)

(defn get
  "Do a get request"
  [url]
  (let [data (request url)]
    (put data :json (fn [self] (json/decode (self :text))))))
